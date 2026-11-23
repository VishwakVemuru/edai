// Runs problems through edai's WebAssembly brain (the copy embedded in runners/browser/index.html), with node.
// Same input format as native_solve; output per line:  0 by v0 v1 -1 -1   (the page exposes only who answered and the answers)
import { readFileSync } from "fs";
import { createInterface } from "readline";
const html = readFileSync(process.env.EDAI_REPO + "/runners/browser/index.html", "utf8");
const b64 = html.match(/WASM_B64\s*=\s*"([^"]+)"/)[1];
const { instance } = await WebAssembly.instantiate(Buffer.from(b64, "base64"), {});
const X = instance.exports; X._initialize();
if (X.selftest() !== 0) { console.error("selftest failed"); process.exit(2); }
const io = () => new BigInt64Array(X.memory.buffer, X.io(), 40);
const rl = createInterface({ input: process.stdin });
for await (const line of rl) {
  const t = line.trim().split(/\s+/); if (!t[0]) continue;
  const a = io(); a.fill(0n); let k = 0, i = 1;
  const n = Number(t[i++]);
  if (t[0] === "R") {
    a[k++] = 0n; a[k++] = BigInt(n);
    const xs = t.slice(i, i + n); i += n; const ys = t.slice(i, i + n); i += n;
    for (const v of xs) a[k++] = BigInt(v); for (const v of ys) a[k++] = BigInt(v);
    const nq = Number(t[i++]); a[k++] = BigInt(nq); for (let j = 0; j < nq; j++) a[k++] = BigInt(t[i++]);
  } else { a[k++] = 1n; a[k++] = BigInt(n); for (let j = 0; j < n; j++) a[k++] = BigInt(t[i++]); }
  X.answer(); const r = io();
  console.log(`0 ${r[0]} ${r[1]} ${r[2]} -1 -1`);
}
