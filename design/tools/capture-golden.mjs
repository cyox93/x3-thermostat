// Capture every golden state as an exact 240×240 PNG (DPR 1) with headless Chrome.
// usage: node capture-golden.mjs <golden.html url> <outDir>
import puppeteer from 'puppeteer';
import fs from 'node:fs';

const [, , url = 'http://localhost:8080/golden.html', outDir = 'golden'] = process.argv;
fs.mkdirSync(outDir, { recursive: true });

const browser = await puppeteer.launch({ args: ['--font-render-hinting=none', '--disable-lcd-text', '--force-color-profile=srgb'] });
const page = await browser.newPage();
await page.setViewport({ width: 240, height: 240, deviceScaleFactor: 1 });

await page.goto(url, { waitUntil: 'networkidle0' });
await page.waitForFunction('window.__GOLDEN_READY === true', { timeout: 30000 });
const ids = await page.evaluate(() => window.__GOLDEN_IDS);
fs.writeFileSync(`${outDir}/states.json`, JSON.stringify(await page.evaluate(() => window.__GOLDEN), null, 2));

for (const id of ids) {
  await page.goto(`${url}?state=${encodeURIComponent(id)}`, { waitUntil: 'networkidle0' });
  await page.waitForFunction('window.__GOLDEN_READY === true', { timeout: 30000 });
  await page.screenshot({ path: `${outDir}/${id}.png`, clip: { x: 0, y: 0, width: 240, height: 240 } });
  console.log('✓', id);
}
await browser.close();
console.log(`${ids.length} golden frames → ${outDir}/`);
