// Short-code registry. Uses Vercel KV (Upstash Redis REST) when its env vars are present, and falls
// back to a stateless self-encoding code when they're not — so the tool works before a KV store is
// attached (longer '~' codes), and upgrades to short 5-char codes the moment KV is connected.
const crypto = require('crypto');
const { isIpv4, isIpv6 } = require('./crewurl.js');

const ALPHABET = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789'; // no 0/O/1/I/L — unambiguous to read + type
const TTL_SECONDS = 6 * 3600; // a session code lives 6 hours

function kvEnabled() {
  return !!(process.env.KV_REST_API_URL && process.env.KV_REST_API_TOKEN);
}

// Upstash REST: POST the command as a JSON array; returns { result } or { error }.
async function kvCommand(args) {
  const url = process.env.KV_REST_API_URL;
  const token = process.env.KV_REST_API_TOKEN;
  const r = await fetch(url, {
    method: 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(args),
  });
  const j = await r.json().catch(() => ({}));
  if (j && j.error) throw new Error(j.error);
  return j;
}

function randomCode(len = 5) {
  const b = crypto.randomBytes(len);
  let s = '';
  for (let i = 0; i < len; i++) s += ALPHABET[b[i] % ALPHABET.length];
  return s;
}

function encodeStateless(url) {
  return '~' + Buffer.from(url, 'utf8').toString('base64url');
}
function decodeStateless(code) {
  try {
    return Buffer.from(code.slice(1), 'base64url').toString('utf8');
  } catch (e) {
    return null;
  }
}

/** Store a (validated) crew URL and return a join code. Short random code with KV; stateless without. */
async function registerUrl(url) {
  if (!kvEnabled()) return { code: encodeStateless(url), stateless: true };
  for (let i = 0; i < 6; i++) {
    const code = randomCode(5);
    const cur = await kvCommand(['GET', 'pj:' + code]);
    if (!cur || cur.result == null) {
      await kvCommand(['SET', 'pj:' + code, url, 'EX', String(TTL_SECONDS)]);
      return { code, stateless: false };
    }
  }
  throw new Error('Code space busy — try again');
}

// Only ever hand back a real crew URL — guards a malformed '~' code (junk base64) or any tampered
// KV value from turning into an arbitrary redirect target.
function looksLikeCrewUrl(u) {
  if (typeof u !== 'string') return false;
  const m = u.match(/^http:\/\/([^/]+)\/stations\?pin=/);
  if (!m) return false;
  const host = m[1];
  const ip = host[0] === '[' ? (host.match(/^\[([^\]]+)\]/) || [])[1] || '' : host.split(':')[0];
  return isIpv4(ip) || isIpv6(ip); // IP-only, like buildCrewUrl — never a domain
}

/** Resolve a join code back to its crew URL, or null if unknown/expired/invalid. */
async function resolveCode(code) {
  if (!code) return null;
  let url = null;
  if (code[0] === '~') {
    url = decodeStateless(code);
  } else if (kvEnabled()) {
    const j = await kvCommand(['GET', 'pj:' + String(code).toUpperCase()]);
    url = j ? j.result : null;
  }
  return looksLikeCrewUrl(url) ? url : null;
}

module.exports = { kvEnabled, registerUrl, resolveCode, TTL_SECONDS };
