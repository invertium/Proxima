// GET /api/join?host=<ip:port>&pin=<pin>  → 307 redirect to the crew console.
// A stable shareable link the host can send: opening it forwards straight to their ship.
const { buildCrewUrl } = require('../lib/crewurl.js');

module.exports = (req, res) => {
  try {
    const q = req.query || {};
    const url = buildCrewUrl(q.host, q.pin);
    res.writeHead(307, { Location: url, 'Cache-Control': 'no-store' });
    res.end();
  } catch (e) {
    res.status(400).json({ ok: false, error: e.message });
  }
};
