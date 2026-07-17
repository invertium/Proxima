// POST /api/register  { host: "<ip:port>", pin: "<pin>" }  → { ok, code, stateless, kv, ttl, url }
// The game (or a host) registers its crew URL and gets a short join code the crew can type.
const { buildCrewUrl } = require('../lib/crewurl.js');
const { registerUrl, kvEnabled, TTL_SECONDS } = require('../lib/store.js');

module.exports = async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  res.setHeader('Cache-Control', 'no-store');
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method !== 'POST') return res.status(405).json({ ok: false, error: 'POST only' });

  let body = req.body;
  if (typeof body === 'string') { try { body = JSON.parse(body); } catch (e) { body = {}; } }
  if (!body || typeof body !== 'object') body = {};

  try {
    const url = buildCrewUrl(body.host, body.pin); // IP-only validation happens here
    const { code, stateless } = await registerUrl(url);
    res.status(200).json({ ok: true, code, stateless, kv: kvEnabled(), ttl: TTL_SECONDS, url });
  } catch (e) {
    res.status(400).json({ ok: false, error: e.message });
  }
};
