// GET /api/whoami → { ip } — the requester's public IP, so a host can grab the address
// to share with crew joining over the internet (LAN players use their LAN IP instead).
module.exports = (req, res) => {
  const fwd = String(req.headers['x-forwarded-for'] || '').split(',')[0].trim();
  const ip = fwd || (req.socket && req.socket.remoteAddress) || '';
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Cache-Control', 'no-store');
  res.status(200).json({ ip });
};
