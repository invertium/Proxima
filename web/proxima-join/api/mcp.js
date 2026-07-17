// Minimal, stateless MCP server over Streamable HTTP (POST → single JSON-RPC response).
// Exposes one tool, `crew_join_url`, so an MCP client (e.g. a Claude connector) can resolve a
// Proxima crew-console URL from a host IP + PIN. No sessions, no storage — the tool is pure.
//
// Add it in an MCP client as a Streamable HTTP server pointing at:  https://<deployment>/api/mcp
const { buildCrewUrl } = require('../lib/crewurl.js');

const TOOL = {
  name: 'crew_join_url',
  description:
    'Build the Proxima crew-console URL to join a friend’s ship. Returns ' +
    'http://<ip:port>/stations?pin=<pin>; open it in a browser to man a bridge station. ' +
    'The host must be an IP address (LAN or public) and the PIN is shown on the ship’s screen.',
  inputSchema: {
    type: 'object',
    properties: {
      host: { type: 'string', description: 'Host IP and port, e.g. "192.168.1.10:8081"' },
      pin: { type: 'string', description: 'Session PIN (digits) shown on the ship’s screen' },
    },
    required: ['host'],
  },
};

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, GET, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Mcp-Session-Id, Mcp-Protocol-Version, Authorization');
  res.setHeader('Access-Control-Expose-Headers', 'Mcp-Session-Id');
  res.setHeader('Cache-Control', 'no-store');

  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method === 'GET') {
    return res.status(200).json({ server: 'proxima-join', transport: 'streamable-http', endpoint: '/api/mcp', tools: [TOOL.name] });
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
      return rpc({ protocolVersion: pv, capabilities: { tools: {} }, serverInfo: { name: 'proxima-join', version: '1.0.0' } });
    }
    if (typeof method === 'string' && method.startsWith('notifications/')) {
      return res.status(202).end();
    }
    if (method === 'ping') return rpc({});
    if (method === 'tools/list') return rpc({ tools: [TOOL] });
    if (method === 'tools/call') {
      if (params.name !== TOOL.name) return rpcErr(-32602, `Unknown tool: ${params.name}`);
      const args = params.arguments || {};
      try {
        const url = buildCrewUrl(args.host, args.pin);
        return rpc({ content: [{ type: 'text', text: url }] });
      } catch (e) {
        return rpc({ content: [{ type: 'text', text: `Error: ${e.message}` }], isError: true });
      }
    }
    return rpcErr(-32601, `Method not found: ${method}`);
  } catch (e) {
    return rpcErr(-32603, `Internal error: ${e.message}`);
  }
};
