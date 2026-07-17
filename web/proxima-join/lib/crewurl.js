// Shared validator + URL builder for the Proxima crew-join redirector.
//
// Deliberately IP-ONLY: the host must be an IPv4/IPv6 literal (LAN or public), never a hostname.
// Combined with the fixed "/stations" path, that stops this from being abused as an open redirector
// to arbitrary phishing domains — it can only ever forward to a game host's own IP address.

function isIpv4(s) {
  return /^(\d{1,3})(\.\d{1,3}){3}$/.test(s) && s.split('.').every((n) => +n >= 0 && +n <= 255);
}
function isIpv6(s) {
  return /^[0-9a-fA-F:]+$/.test(s) && (s.match(/:/g) || []).length >= 2;
}

/** Build http://<ip:port>/stations?pin=<pin> from user input, or throw a friendly Error. */
function buildCrewUrl(hostInput, pinInput) {
  let host = String(hostInput || '').trim().replace(/^https?:\/\//i, '').replace(/\/.*$/, '');
  const pin = String(pinInput == null ? '' : pinInput).trim();
  if (!host) throw new Error('Enter the host address shown on the ship’s screen');

  let ip;
  let port = '8080';
  if (host[0] === '[') {
    const m = host.match(/^\[([^\]]+)\](?::(\d+))?$/);
    if (!m) throw new Error('Malformed IPv6 address');
    ip = m[1];
    if (m[2]) port = m[2];
  } else if ((host.match(/:/g) || []).length > 1) {
    ip = host; // bare IPv6 literal, no port → default
  } else if (host.includes(':')) {
    [ip, port] = host.split(':');
  } else {
    ip = host;
  }

  if (!(isIpv4(ip) || isIpv6(ip))) {
    throw new Error('Host must be an IP address, e.g. 192.168.1.10:8081');
  }
  if (!/^\d{1,5}$/.test(String(port)) || +port < 1 || +port > 65535) {
    throw new Error('Port must be 1–65535 (the ship’s screen shows it)');
  }
  if (pin && !/^\d{1,8}$/.test(pin)) throw new Error('PIN should be digits only');

  const authority = isIpv6(ip) ? `[${ip}]:${port}` : `${ip}:${port}`;
  return `http://${authority}/stations?pin=${encodeURIComponent(pin)}`;
}

module.exports = { buildCrewUrl, isIpv4, isIpv6 };
