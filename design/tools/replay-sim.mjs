// Replays scenarios.json on the LVGL sim through the test commands (docs/ui.md §6.2-6.4):
// state via DUMP against `expect`, screens via SNAP → out/sim-replay/<scenario>-<snap>.png.
// usage: node replay-sim.mjs [scenarios.json] [outDir] [--port 9999]   (sim must be running)
import net from 'node:net';
import fs from 'node:fs';
import { PNG } from 'pngjs';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const pos = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
const [file = 'scenarios.json', outDir = 'out/sim-replay'] = pos;
const PORT = Number(opt('--port', 9999));
const spec = JSON.parse(fs.readFileSync(file, 'utf8'));
fs.mkdirSync(outDir, { recursive: true });

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

// Same partial match as replay-html.mjs
const match = (exp, act, p = '') => {
  const errs = [];
  for (const k of Object.keys(exp)) {
    const e = exp[k], a = act?.[k];
    if (e && typeof e === 'object') errs.push(...match(e, a, `${p}${k}.`));
    else if (typeof e === 'number' ? Math.abs(e - a) > 1e-6 : e !== a) errs.push(`${p}${k}: expected ${JSON.stringify(e)}, got ${JSON.stringify(a)}`);
  }
  return errs;
};

const EVENTS = { 'enc+': 'ENC+', 'enc-': 'ENC-', short: 'SHORT', long: 'LONG', room: 'ROOM' };
const command = (ev) => (ev && ev.burn) ? `BURN ${ev.burn[0]} ${ev.burn[1] ? 1 : 0}` : EVENTS[ev];

async function snap(path) {
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
  fs.writeFileSync(path, PNG.sync.write(png));
}

let fails = 0;
await ok('ANIM 0');
for (const sc of spec.scenarios) {
  await ok('RESET');
  const initErr = match(spec.initial, JSON.parse(await send('DUMP')));
  if (initErr.length) { console.log(`✗ ${sc.name} [initial]`, initErr); fails++; continue; }
  let i = 0;
  for (const step of sc.steps) {
    i++;
    for (const ev of step.do) await ok(command(ev));
    const errs = step.expect ? match(step.expect, JSON.parse(await send('DUMP'))) : [];
    if (errs.length) { fails++; console.log(`✗ ${sc.name} step ${i}`, errs); } else console.log(`✓ ${sc.name} step ${i}`);
    if (step.snap) await snap(`${outDir}/${sc.name}-${step.snap}.png`);
  }
}
console.log(fails ? `✗ ${fails} failures` : '✓ all scenarios passed');
process.exit(fails ? 1 : 0);
