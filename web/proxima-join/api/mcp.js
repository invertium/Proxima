// Minimal, stateless MCP server over Streamable HTTP (POST → single JSON-RPC response).
// Tools:
//   crew_join_url(host, pin)   → build the crew URL from an IP + PIN (pure).
//   register_host(host, pin)   → register a host and get a short join code (needs KV; stateless code otherwise).
//   resolve_join_code(code)    → resolve a join code back to its crew URL.
// Add it in an MCP client as a Streamable HTTP server pointing at:  https://<deployment>/api/mcp
const { buildCrewUrl } = require('../lib/crewurl.js');
const { registerUrl, resolveCode } = require('../lib/store.js');

const TOOLS = [
  {
    name: 'crew_join_url',
    description:
      'Build the Proxima crew-console URL from a host IP + PIN. Returns http://<ip:port>/stations?pin=<pin>; ' +
      'open it in a browser to man a bridge station. Host must be an IP address (LAN or public).',
    inputSchema: {
      type: 'object',
      properties: {
        host: { type: 'string', description: 'Host IP and port, e.g. "192.168.1.10:8081"' },
        pin: { type: 'string', description: 'Session PIN (digits) shown on the ship’s screen' },
      },
      required: ['host'],
    },
  },
  {
    name: 'register_host',
    description:
      'Register a Proxima host (IP + PIN) and get a short join code the crew can type at ' +
      'https://proxima-join.vercel.app. Codes expire after 6 hours.',
    inputSchema: {
      type: 'object',
      properties: {
        host: { type: 'string', description: 'Host IP and port, e.g. "192.168.1.10:8081"' },
        pin: { type: 'string', description: 'Session PIN (digits)' },
      },
      required: ['host'],
    },
  },
  {
    name: 'resolve_join_code',
    description: 'Resolve a Proxima join code back to the ship’s crew-console URL.',
    inputSchema: {
      type: 'object',
      properties: { code: { type: 'string', description: 'The join code shown on the ship’s screen' } },
      required: ['code'],
    },
  },
];
const TOOL_NAMES = TOOLS.map((t) => t.name);

async function callTool(name, args) {
  args = args || {};
  if (name === 'crew_join_url') return buildCrewUrl(args.host, args.pin);
  if (name === 'register_host') {
    const url = buildCrewUrl(args.host, args.pin);
    const { code } = await registerUrl(url);
    return `Join code: ${code}\nCrew: open https://proxima-join.vercel.app and enter ${code} (or https://proxima-join.vercel.app/j/${code}).`;
  }
  if (name === 'resolve_join_code') {
    const url = await resolveCode(args.code);
    if (!url) throw new Error('Unknown or expired join code');
    return url;
  }
  throw new Error(`Unknown tool: ${name}`);
}

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, GET, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Mcp-Session-Id, Mcp-Protocol-Version, Authorization');
  res.setHeader('Access-Control-Expose-Headers', 'Mcp-Session-Id');
  res.setHeader('Cache-Control', 'no-store');

  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method === 'GET') {
    return res.status(200).json({ server: 'proxima-join', transport: 'streamable-http', endpoint: '/api/mcp', tools: TOOL_NAMES });
  }
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });

  let body = req.body;
  if (typeof body === 'string') { try { body = JSON.parse(body); } catch (e) { body = {}; } }
  if (!body || typeof body !== 'object') body = {};

  const { method, id = null, params = {} } = body;
  const rpc = (result) => res.status(200).json({ jsonrpc: '2.0', id, result });
  const rpcErr = (code, message) => res.status(200).json({ jsonrpc: '2.0', id, error: { code, message } });

  try {
    if (method === 'initialize') {
      const pv = (params && params.protocolVersion) || '2024-11-05';
      return rpc({ protocolVersion: pv, capabilities: { tools: {} }, serverInfo: { name: 'proxima-join', version: '1.1.0' } });
    }
    if (typeof method === 'string' && method.startsWith('notifications/')) return res.status(202).end();
    if (method === 'ping') return rpc({});
    if (method === 'tools/list') return rpc({ tools: TOOLS });
    if (method === 'tools/call') {
      if (!TOOL_NAMES.includes(params.name)) return rpcErr(-32602, `Unknown tool: ${params.name}`);
      try {
        const text = await callTool(params.name, params.arguments);
        return rpc({ content: [{ type: 'text', text }] });
      } catch (e) {
        return rpc({ content: [{ type: 'text', text: `Error: ${e.message}` }], isError: true });
      }
    }
    return rpcErr(-32601, `Method not found: ${method}`);
  } catch (e) {
    return rpcErr(-32603, `Internal error: ${e.message}`);
  }
};
