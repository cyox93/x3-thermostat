// Renders golden states in the LVGL sim and saves out/lvgl/<id>.png (docs/ui.md §6.3-6.4).
// usage: node render-sim.mjs [--ids a,b,c] [--port 9999]   (sim must be running)
import net from 'node:net';
import fs from 'node:fs';
import { PNG } from 'pngjs';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const PORT = Number(opt('--port', 9999));
const IDS = opt('--ids', null)?.split(',');
const OUT = 'out/lvgl';
fs.mkdirSync(OUT, { recursive: true });

// Same constants as design/src/golden-states.jsx
const TEMPS = { '거실': 21.4, '큰방': 19.8, '일하는방': 20.6, '노는방': 22.1 };
const HUMS = { '거실': 45, '큰방': 52, '일하는방': 38, '노는방': 47 };
const x10 = (v) => Math.round(v * 10);

const send = (cmd) => new Promise((resolve, reject) => {
  const s = net.connect(PORT, '127.0.0.1');
  let buf = '';
  s.setEncoding('utf8');
  s.on('data', (d) => { buf += d; });
  s.on('end', () => resolve(buf.trim()));
  s.on('error', reject);
  s.write(cmd + '\n');
});
const ok = async (cmd) => { const r = await send(cmd); if (r !== 'OK') throw new Error(`${cmd} → ${r}`); };

// golden item → state commands (docs/ui.md §6.3); null = screen not built yet
function commands(g) {
  const room = (name, on, target, burning, noHum) =>
    `ROOM ${name} ${on ? 1 : 0} ${x10(target)} ${x10(TEMPS[name])} ${noHum ? '-' : HUMS[name]} ${burning ? 1 : 0}`;
  if (g.kind !== 'main') return null;
  const c = [`INSTALL ${g.summary ? '거실' : g.room}`, `ACTIVE ${g.room}`];
  if (g.summary) for (const r of g.summary) c.push(room(r.name, r.on, 22, r.burning, false));
  c.push(room(g.room, g.on, g.target, g.burning, g.noHum));
  c.push(`WIFI ${g.wifi ?? 3}`, 'MODE main');
  return c;
}

async function snap(id) {
  const r = await send('SNAP');
  if (!r.startsWith('SNAP ')) throw new Error(`SNAP → ${r.slice(0, 40)}`);
  const raw = Buffer.from(r.slice(5), 'base64');
  const png = new PNG({ width: 240, height: 240 });
  for (let i = 0; i < 240 * 240; i++) {
    const v = raw.readUInt16LE(i * 2);
    const r5 = v >> 11, g6 = (v >> 5) & 63, b5 = v & 31;
    png.data[i * 4] = (r5 << 3) | (r5 >> 2);
    png.data[i * 4 + 1] = (g6 << 2) | (g6 >> 4);
    png.data[i * 4 + 2] = (b5 << 3) | (b5 >> 2);
    png.data[i * 4 + 3] = 255;
  }
  fs.writeFileSync(`${OUT}/${id}.png`, PNG.sync.write(png));
}

const states = JSON.parse(fs.readFileSync('golden/states.json', 'utf8'));
await ok('ANIM 0');
for (const g of states) {
  if (IDS && !IDS.includes(g.id)) continue;
  const cmds = commands(g);
  if (!cmds) { console.log('-', g.id, '(not implemented)'); continue; }
  await ok('RESET');
  for (const cmd of cmds) await ok(cmd);
  await snap(g.id);
  console.log('✓', g.id);
}
