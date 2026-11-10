// Deterministic 240×240 golden frames. ?state=<id> renders one frame at (0,0); no param = gallery.
// Animations are disabled by golden.html CSS. Brightness is NOT applied (LVGL uses backlight PWM).
const R = ['거실', '큰방', '작은방', '일하는방'];
const HUMS = { '거실': 45, '큰방': 52, '작은방': 47, '일하는방': 38 };
const TEMPS = { '거실': 21.4, '큰방': 19.8, '작은방': 22.1, '일하는방': 20.6 };
const summary = (on, burn, ) => R.map((r, i) => ({ name: r, on: on[i], burning: burn[i] }));
const SET = (sel, room = '거실') => ({ kind: 'settings', sel, settings: { roomName: room, brightness: 70, screenTime: 30 } });

const GOLDEN = [
  { id: 'main-on',            kind: 'main', room: '큰방', on: true,  target: 22.0, burning: false },
  { id: 'main-on-burning',    kind: 'main', room: '큰방', on: true,  target: 22.0, burning: true },
  { id: 'main-off',           kind: 'main', room: '큰방', on: false, target: 22.0, burning: false },
  { id: 'main-on-min',        kind: 'main', room: '큰방', on: true,  target: 15.0, burning: false },
  { id: 'main-on-max',        kind: 'main', room: '큰방', on: true,  target: 30.0, burning: true },
  { id: 'master-living-burn', kind: 'main', room: '거실', on: true,  target: 22.0, burning: true,  summary: summary([1,0,0,1], [1,0,0,0]) },
  { id: 'master-idle',        kind: 'main', room: '거실', on: true,  target: 22.0, burning: false, summary: summary([1,0,0,0], [0,0,0,0]) },
  { id: 'master-work-active', kind: 'main', room: '일하는방', on: true, target: 23.5, burning: true, summary: summary([1,1,1,1], [0,1,0,1]) },
  { id: 'master-off',         kind: 'main', room: '작은방', on: false, target: 22.0, burning: false, summary: summary([0,0,0,0], [0,0,0,0]) },
  { id: 'settings-0-room',    ...SET(0) },
  { id: 'settings-1-bright',  ...SET(1) },
  { id: 'settings-2-time',    ...SET(2) },
  { id: 'settings-3-back',    ...SET(3) },
  ...R.map((r, i) => ({ id: `room-edit-${i}`, kind: 'room', sel: i })),
  { id: 'bright-10',  kind: 'bright', value: 10 },
  { id: 'bright-70',  kind: 'bright', value: 70 },
  { id: 'bright-100', kind: 'bright', value: 100 },
  ...[10, 20, 30, 60, 120, 300].map(v => ({ id: `time-${v}`, kind: 'time', value: v })),
  { id: 'main-no-humidity', kind: 'main', room: '큰방', on: true, target: 22.0, burning: false, noHum: true },
  { id: 'wifi-0', kind: 'main', room: '큰방', on: true, target: 22.0, burning: false, wifi: 0 },
  { id: 'wifi-1', kind: 'main', room: '큰방', on: true, target: 22.0, burning: false, wifi: 1 },
];

function Frame({ s }) {
  let body = null;
  if (s.kind === 'main') body = <window.ScreenBoiler roomName={s.room} currentTemp={TEMPS[s.room]} humidity={s.noHum || (s.summary && s.room !== '거실') ? null : HUMS[s.room]} state={{ on: s.on, target: s.target }} burning={s.burning} summary={s.summary || null} />;
  if (s.kind === 'settings') body = <window.ScreenSettings items={window.SETTINGS_ITEMS} selected={s.sel} settings={s.settings} />;
  if (s.kind === 'room') body = <window.ScreenRoomEdit selected={s.sel} />;
  if (s.kind === 'bright') body = <window.ScreenBrightness value={s.value} />;
  if (s.kind === 'time') body = <window.ScreenScreenTime value={s.value} />;
  return <div className="lcd" data-golden={s.id}>{body}<window.StatusBar wifi={s.wifi ?? 3} /></div>;
}

(async () => {
  await new Promise(r => { const t = () => (window.ScreenBoiler && window.StatusBar) ? r() : requestAnimationFrame(t); t(); });
  try { await document.fonts.ready; } catch (e) {}
  const id = new URLSearchParams(location.search).get('state');
  const root = ReactDOM.createRoot(document.getElementById('golden'));
  if (id) {
    const s = GOLDEN.find(g => g.id === id);
    document.body.className = 'single';
    root.render(s ? <Frame s={s} /> : <div style={{ padding: 8 }}>unknown state: {id}</div>);
  } else {
    root.render(<div className="gallery">{GOLDEN.map(s => <figure key={s.id}><Frame s={s} /><figcaption>{s.id}</figcaption></figure>)}</div>);
  }
  await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
  try { await document.fonts.ready; } catch (e) {}
  window.__GOLDEN_IDS = GOLDEN.map(g => g.id);
  window.__GOLDEN = GOLDEN;
  window.__GOLDEN_READY = true;
})();
