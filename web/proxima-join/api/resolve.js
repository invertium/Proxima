// GET /api/resolve?code=<code>  (also reachable as /j/<code> via a rewrite)
// Looks the code up and 307-redirects to the ship's crew consoles, or shows a friendly "not found".
const { resolveCode } = require('../lib/store.js');

module.exports = async (req, res) => {
  res.setHeader('Cache-Control', 'no-store');
  const code = (req.query && (req.query.code || req.query.c)) || '';
  let url = null;
  try { url = await resolveCode(code); } catch (e) { url = null; }

  if (url) {
    res.writeHead(307, { Location: url, 'Cache-Control': 'no-store' });
    return res.end();
  }
  res.status(404).setHeader('Content-Type', 'text/html');
  res.end(
    "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>" +
    "<title>Code not found</title>" +
    "<body style='background:radial-gradient(1000px 500px at 50% -10%,#0f2338,#05080f);color:#cfe6ff;" +
    "font-family:system-ui;min-height:100vh;margin:0;display:grid;place-items:center;text-align:center'>" +
    "<div><h2 style='letter-spacing:.1em'>CODE NOT FOUND</h2>" +
    "<p style='color:#7f97b6'>That join code is unknown or has expired.<br>Ask the host for a fresh one.</p>" +
    "<p><a style='color:#7ad0ff' href='/'>&larr; enter another code</a></p></div></body>"
  );
};
