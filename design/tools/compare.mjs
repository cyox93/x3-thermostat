// Pixel diff: golden/*.png vs actual/*.png (same file names), only inside the round LCD (r=120).
// usage: node compare.mjs <goldenDir> <actualDir> <diffDir> [--threshold 0.1] [--max 2.0]
//   --threshold : per-pixel color tolerance for pixelmatch (0..1)
//   --max       : allowed % of differing pixels inside the circle before FAIL
import fs from 'node:fs';
import path from 'node:path';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';

const args = process.argv.slice(2);
const [goldenDir = 'golden', actualDir = 'out/lvgl', diffDir = 'out/diff'] = args.filter(a => !a.startsWith('--'));
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? Number(args[i + 1]) : d; };
const THRESH = opt('--threshold', 0.1), MAX_PCT = opt('--max', 2.0);
fs.mkdirSync(diffDir, { recursive: true });

const W = 240, H = 240, CX = 119.5, CY = 119.5, RAD = 120;
const read = (p) => PNG.sync.read(fs.readFileSync(p));
const maskOutside = (png) => {
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    if ((x - CX) ** 2 + (y - CY) ** 2 > RAD * RAD) { const i = (y * W + x) * 4; png.data[i] = png.data[i + 1] = png.data[i + 2] = 0; png.data[i + 3] = 255; }
  }
};
let inside = 0;
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if ((x - CX) ** 2 + (y - CY) ** 2 <= RAD * RAD) inside++;

const files = fs.readdirSync(goldenDir).filter(f => f.endsWith('.png'));
// Stage-by-stage: compare only what the implementation has rendered so far
const PRESENT_ONLY = args.includes('--present-only');
const rows = []; let fails = 0;
for (const f of files) {
  const ap = path.join(actualDir, f);
  if (!fs.existsSync(ap)) { if (PRESENT_ONLY) continue; rows.push([f, 'MISSING', '-']); fails++; continue; }
  const g = read(path.join(goldenDir, f)), a = read(ap);
  if (a.width !== W || a.height !== H) { rows.push([f, `SIZE ${a.width}x${a.height}`, '-']); fails++; continue; }
  maskOutside(g); maskOutside(a);
  const diff = new PNG({ width: W, height: H });
  const n = pixelmatch(g.data, a.data, diff.data, W, H, { threshold: THRESH, includeAA: false });
  fs.writeFileSync(path.join(diffDir, f), PNG.sync.write(diff));
  const pct = (100 * n / inside);
  const ok = pct <= MAX_PCT; if (!ok) fails++;
  rows.push([f, ok ? 'PASS' : 'FAIL', pct.toFixed(2) + '%']);
}
console.table(rows.map(([file, result, diff]) => ({ file, result, diff })));
console.log(fails ? `✗ ${fails}/${rows.length} failed` : `✓ all ${rows.length} passed`);
process.exit(fails ? 1 : 0);
