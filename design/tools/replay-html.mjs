// Replay scenarios.json against the HTML simulator in Chrome; check state + snap the LCD.
// The SAME scenarios.json is replayed on firmware via the serial test protocol (see TESTING.md).
// usage: node replay-html.mjs <index.html url> <scenarios.json> <outDir>
import puppeteer from 'puppeteer';
import fs from 'node:fs';

const [, , url = 'http://localhost:8080/index.html', file = 'scenarios.json', outDir = 'out/html'] = process.argv;
const spec = JSON.parse(fs.readFileSync(file, 'utf8'));
fs.mkdirSync(outDir, { recursive: true });

const match = (exp, act, p = '') => {
  const errs = [];
  for (const k of Object.keys(exp)) {
    const e = exp[k], a = act?.[k];
    if (e && typeof e === 'object') errs.push(...match(e, a, `${p}${k}.`));
    else if (typeof e === 'number' ? Math.abs(e - a) > 1e-6 : e !== a) errs.push(`${p}${k}: expected ${JSON.stringify(e)}, got ${JSON.stringify(a)}`);
  }
  return errs;
};

const browser = await puppeteer.launch({ args: ['--font-render-hinting=none', '--disable-lcd-text', '--force-color-profile=srgb'] });
const page = await browser.newPage();
await page.setViewport({ width: 1200, height: 900, deviceScaleFactor: 1 });
await page.addStyleTag({ content: '' }).catch(() => {});
let fails = 0;

for (const sc of spec.scenarios) {
  await page.goto(`${url}?test=1`, { waitUntil: 'networkidle0' });
  await page.waitForFunction('window.__THERMO_READY === true', { timeout: 30000 });
  await page.addStyleTag({ content: '*{animation:none!important;transition:none!important}' });
  const init = await page.evaluate(() => window.__thermo.getState());
  const initErr = match(spec.initial, init);
  if (initErr.length) { console.log(`✗ ${sc.name} [initial]`, initErr); fails++; continue; }

  let i = 0;
  for (const step of sc.steps) {
    i++;
    for (const ev of step.do) { await page.evaluate((e) => window.__thermo.dispatch(e), ev); await new Promise(r => setTimeout(r, 30)); }
    await new Promise(r => setTimeout(r, 80));
    const st = await page.evaluate(() => window.__thermo.getState());
    const errs = step.expect ? match(step.expect, st) : [];
    if (errs.length) { fails++; console.log(`✗ ${sc.name} step ${i}`, errs); } else console.log(`✓ ${sc.name} step ${i}`);
    if (step.snap) {
      const el = await page.$('#lcd');
      await el.screenshot({ path: `${outDir}/${sc.name}-${step.snap}.png` });
    }
  }
}
await browser.close();
console.log(fails ? `✗ ${fails} failures` : '✓ all scenarios passed');
process.exit(fails ? 1 : 0);
