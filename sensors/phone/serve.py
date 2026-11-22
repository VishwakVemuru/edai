"""Serves the edai sensor page to the iPhone over https (motion sensors need a secure page) and keeps what it sends.

    python3 serve.py [local_port] [phone_port]        (defaults 8961 and 8962)

  http://127.0.0.1:<local_port>/      this Mac's own browser
  https://<this Mac>:<phone_port>/    the phone on the same Wi-Fi; the certificate is made here (in .cert/, not in any
                                      repo), so the phone warns once and you tap through
  POST /save                          the page's readings and the brain's answers, written to ../out/phone/"""
import datetime
import ipaddress
import json
import os
import re
import socket
import ssl
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, "index.html")
OUT = os.path.join(HERE, "..", "out", "phone")
CERT = os.path.join(HERE, ".cert")


def addresses():
    """the Mac's addresses a phone on the same network could use (no loopback, no link-local, no 464XLAT address)"""
    out = []
    txt = subprocess.run(["ifconfig"], capture_output=True, text=True).stdout
    for line in txt.splitlines():
        m = re.search(r"inet6? ([0-9a-fA-F:.]+)", line)
        if not m:
            continue
        a = m.group(1)
        try:
            ip = ipaddress.ip_address(a)
        except ValueError:
            continue
        if ip.is_loopback or ip.is_link_local or "clat46" in line or a.startswith("192.0.0."):
            continue
        out.append((a, "temporary" in line))
    return out


def make_cert(host, ips):
    os.makedirs(CERT, exist_ok=True)
    want = sorted(set(ips + ["127.0.0.1"]))
    stamp = os.path.join(CERT, "for.json")
    if os.path.exists(stamp) and json.load(open(stamp)) == {"host": host, "ips": want} and os.path.exists(os.path.join(CERT, "cert.pem")):
        return
    alt = ["DNS.1 = localhost", f"DNS.2 = {host}.local"] + [f"IP.{i + 1} = {a}" for i, a in enumerate(want)]
    cfg = os.path.join(CERT, "openssl.cnf")
    open(cfg, "w").write("\n".join([
        "[req]", "distinguished_name = dn", "x509_extensions = ext", "prompt = no", "[dn]", "CN = edai sensor test", "[ext]",
        "basicConstraints = critical, CA:FALSE", "keyUsage = critical, digitalSignature, keyEncipherment", "extendedKeyUsage = serverAuth",
        "subjectAltName = @alt", "[alt]"] + alt) + "\n")
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "30", "-config", cfg,
                    "-keyout", os.path.join(CERT, "key.pem"), "-out", os.path.join(CERT, "cert.pem")], check=True, capture_output=True)
    os.chmod(os.path.join(CERT, "key.pem"), 0o600)
    json.dump({"host": host, "ips": want}, open(stamp, "w"))


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stdout.write("%s %s %s\n" % (datetime.datetime.now().strftime("%H:%M:%S"), self.client_address[0], fmt % args)); sys.stdout.flush()

    def send(self, code, body, ctype="application/json"):
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store"); self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        if self.path.split("?")[0] not in ("/", "/index.html"):
            return self.send(404, b'{"error": "not found"}')
        self.send(200, open(PAGE, "rb").read(), "text/html; charset=utf-8")

    def do_POST(self):
        if self.path != "/save":
            return self.send(404, b'{"error": "not found"}')
        n = int(self.headers.get("Content-Length") or 0)
        if n <= 0 or n > 20 << 20:
            return self.send(413, b'{"error": "too large"}')
        body = self.rfile.read(n)
        try:
            json.loads(body)
        except Exception:
            return self.send(400, b'{"error": "not json"}')
        os.makedirs(OUT, exist_ok=True)
        name = datetime.datetime.now().strftime("%Y%m%d-%H%M%S") + "-motion.json"
        open(os.path.join(OUT, name), "wb").write(body)
        print("saved", name, "from", self.client_address[0], flush=True)
        self.send(200, json.dumps({"ok": True, "file": name}).encode())


class Local(ThreadingHTTPServer):
    daemon_threads = True


class Dual(ThreadingHTTPServer):
    address_family = socket.AF_INET6
    daemon_threads = True

    def server_bind(self):
        self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()


def main():
    lp = int(sys.argv[1]) if len(sys.argv) > 1 else 8961
    pp = int(sys.argv[2]) if len(sys.argv) > 2 else 8962
    host = subprocess.run(["scutil", "--get", "LocalHostName"], capture_output=True, text=True).stdout.strip() or "localhost"
    addrs = addresses()
    make_cert(host, [a for a, _ in addrs])
    local = Local(("127.0.0.1", lp), Handler)
    phone = Dual(("::", pp), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(os.path.join(CERT, "cert.pem"), os.path.join(CERT, "key.pem"))
    phone.socket = ctx.wrap_socket(phone.socket, server_side=True, do_handshake_on_connect=False)
    threading.Thread(target=local.serve_forever, daemon=True).start()
    print(f"this Mac:   http://127.0.0.1:{lp}/", flush=True)
    for a, temp in addrs:
        print(f"phone:      https://{('[' + a + ']') if ':' in a else a}:{pp}/{'   (temporary address)' if temp else ''}", flush=True)
    try:
        phone.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
