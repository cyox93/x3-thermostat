// Round Remote — Boiler (1.28" 240×240 LCD · rotary encoder + single push button)
//
// Single-room build. Room is set in firmware; not switchable from device.
//
// Interaction — no touch:
//   • Encoder rotate                → main: target temp ±0.5° · settings: scroll / edit value
//   • Short press button            → main: ON/OFF toggle · settings: enter item / commit / back
//   • Long press button (>500ms)    → main: open settings
//
// Settings items: 화면밝기, 화면시간, 이전 (exit)

const { useState, useRef, useEffect, useCallback } = React;

const QA = {
  bg: '#0e0e10',
  bg2: '#4a4a52',
  fg: '#ffffff',
  dim: '#d0d0d4',
};
const BOILER_ACCENT = '#e88a5e';
const SETTING_ACCENT = '#cfcfcf';

const VB = 240;
const CX = 120, CY = 120;

const SCREEN_TIME_STEPS = [10, 20, 30, 60, 120, 300]; // seconds
const ROOMS = ['거실', '큰방', '작은방', '일하는방'];

// ─── arc helper ──────────────────────────────────────────────
function arcPath(cx, cy, r, startDeg, endDeg) {
  const toRad = (d) => (d - 90) * Math.PI / 180;
  const x1 = cx + r * Math.cos(toRad(startDeg));
  const y1 = cy + r * Math.sin(toRad(startDeg));
  const x2 = cx + r * Math.cos(toRad(endDeg));
  const y2 = cy + r * Math.sin(toRad(endDeg));
  const large = Math.abs(endDeg - startDeg) > 180 ? 1 : 0;
  const sweep = endDeg > startDeg ? 1 : 0;
  return `M ${x1} ${y1} A ${r} ${r} 0 ${large} ${sweep} ${x2} ${y2}`;
}

// ─── status bar (wifi) at bottom of round LCD ──────
function StatusBar({ wifi }) {
  return (
    <div style={{
      position: 'absolute', bottom: 22, left: 0, right: 0,
      display: 'flex', justifyContent: 'center', alignItems: 'center', gap: 10,
      zIndex: 6, pointerEvents: 'none',
      fontFamily: 'JetBrains Mono', fontSize: 9, color: QA.dim, letterSpacing: '0.08em',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
        <svg width="14" height="12" viewBox="0 0 13 11">
          {[3, 2, 1].map((_, i) => {
            const r = 3 + i * 2;
            const active = wifi >= 3 - i;
            return (
              <path key={i}
                d={`M ${6.5 - r} 10 A ${r} ${r} 0 0 1 ${6.5 + r} 10`}
                fill="none" stroke={active ? QA.fg : QA.dim} strokeWidth="1.2" opacity={active ? 0.9 : 0.3}
              />
            );
          })}
          <circle cx="6.5" cy="10" r="0.9" fill={QA.fg} />
        </svg>
        <span>{wifi > 0 ? `${wifi}/3` : 'OFF'}</span>
      </div>
    </div>
  );
}

// ─── MAIN: temperature dial ──────────────────────────────────
// Reading icons on the current line: single colour, the text's colour, 15 px tall
// Thermometer: Material Design Icons "thermometer" (Pictogrammers, Apache-2.0)
function ThermoIcon({ color }) {
  return (
    <svg width="8" height="15" viewBox="6.67 2 10.67 20" style={{ display: 'block' }}>
      <path d="M15 13V5A3 3 0 0 0 9 5V13A5 5 0 1 0 15 13M12 4A1 1 0 0 1 13 5V8H11V5A1 1 0 0 1 12 4Z" fill={color} />
    </svg>
  );
}

function DropIcon({ color }) {
  return (
    <svg width="11" height="15" viewBox="0 0 12 16" style={{ display: 'block' }}>
      <path d="M6 0.6C6 0.6 0.9 6.6 0.9 10.6A5.1 5.1 0 0 0 11.1 10.6C11.1 6.6 6 0.6 6 0.6Z" fill={color} />
    </svg>
  );
}

function FlameIcon({ size = 16, color, still = false }) {
  return (
    <svg width={size} height={size * 1.25} viewBox="0 0 16 20" style={{ display: 'block', animation: still ? 'none' : 'qa-flame 1.4s ease-in-out infinite', transformOrigin: '50% 100%' }}>
      <path d="M8 0.5C8.6 4 12.5 6.2 13.6 10.2C14.8 14.6 11.9 19.5 8 19.5C4.1 19.5 1.2 16.4 1.9 12.3C2.3 10 3.6 8.6 4.6 7.4C4.8 9.4 5.6 10.6 6.6 11C6.2 7.2 7.6 3.6 8 0.5Z" fill={color || BOILER_ACCENT} />
      {!color && <path d="M8.2 10.2C9.9 12 11 13.6 10.6 15.7C10.2 17.6 9.1 18.4 8 18.4C6.6 18.4 5.4 17.3 5.6 15.6C5.8 14.2 6.9 13.4 7.4 12.3C7.8 13.1 8.3 13.5 8.7 13.6C8.7 12.4 8.4 11.3 8.2 10.2Z" fill="#ffd27a" />}
    </svg>
  );
}

function BoilerBadge({ burning }) {
  return (
    <div style={{ position: 'absolute', top: 26, left: 0, right: 0, display: 'flex', justifyContent: 'center', zIndex: 5, pointerEvents: 'none' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4, fontFamily: 'Pretendard', fontSize: 12, fontWeight: 600, letterSpacing: '0.04em', color: burning ? '#ffffff' : '#a8a8ae' }}>
        {/* flame slot always kept so the text never moves; the flame shows only while burning */}
        <div style={{ display: 'flex', visibility: burning ? 'visible' : 'hidden' }}><FlameIcon size={10} still={!burning} /></div>
        보일러
      </div>
    </div>
  );
}

function RoomSummary({ rooms, activeName }) {
  return (
    <div style={{ position: 'absolute', bottom: 44, left: 0, right: 0, display: 'flex', justifyContent: 'center', alignItems: 'center', gap: 3, zIndex: 5, pointerEvents: 'none' }}>
      {rooms.map(r => {
        const active = r.name === activeName;
        const burn = r.on && r.burning;
        const c = r.on ? '#ffffff' : '#9a9aa0';
        return (
          <div key={r.name} style={{ display: 'flex', alignItems: 'center', padding: '1px 4px 1px 2px', borderRadius: 4, border: `1px solid ${active ? '#ffffff' : 'transparent'}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 2, fontFamily: 'Pretendard', fontSize: 12, fontWeight: 600, lineHeight: 1.1, color: c }}>
              <div style={{ width: 9, height: 12, display: 'flex', alignItems: 'center', justifyContent: 'center', visibility: burn ? 'visible' : 'hidden' }}>
                <FlameIcon size={9} color={c} still />
              </div>
              {r.name[0]}
            </div>
          </div>
        );
      })}
    </div>
  );
}

function ScreenBoiler({ roomName, currentTemp, humidity = null, state, burning = false, summary = null }) {
  // flame only when the boiler actually reports burner running (signal from boiler)
  const heating = state.on && burning;
  const min = 15, max = 30;
  const target = state.target;
  const t = (target - min) / (max - min);
  const startDeg = -130, endDeg = 130;
  const valDeg = startDeg + (endDeg - startDeg) * t;

  const tCur = Math.max(0, Math.min(1, (currentTemp - min) / (max - min)));
  const curDeg = startDeg + (endDeg - startDeg) * tCur;
  const curRad = (curDeg - 90) * Math.PI / 180;

  const rArc = 100;
  // current-temp marker straddles the arc, kept well inside the bezel
  const rCurIn = 91, rCurOut = 109;

  return (
    <div style={{ position: 'absolute', inset: 0 }}>
      <style>{`@keyframes qa-flame{0%,100%{transform:scale(1,1)}30%{transform:scale(.92,1.08)}60%{transform:scale(1.05,.94)}}`}</style>
      <svg viewBox={`0 0 ${VB} ${VB}`} style={{ position: 'absolute', inset: 0 }}>
        <path d={arcPath(CX, CY, rArc, startDeg, endDeg)} stroke={QA.bg2} strokeWidth="7" fill="none" />
        {state.on && (
          <path d={arcPath(CX, CY, rArc, startDeg, valDeg)} stroke={BOILER_ACCENT} strokeWidth="8" fill="none" strokeLinecap="round" />
        )}
        <line
          x1={CX + rCurIn * Math.cos(curRad)} y1={CY + rCurIn * Math.sin(curRad)}
          x2={CX + rCurOut * Math.cos(curRad)} y2={CY + rCurOut * Math.sin(curRad)}
          stroke="#7fc8d4" strokeWidth="4" strokeLinecap="round" opacity="0.95"
        />
      </svg>
      <div style={{ position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', paddingTop: 0, paddingBottom: summary ? 16 : 0 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 5, fontFamily: 'Pretendard', fontSize: 20, fontWeight: 500, letterSpacing: '0.13em', color: QA.dim, marginBottom: 5 }}>
          {/* the slot is always kept so the room name never moves; only the flame shows or hides */}
          <div style={{ display: 'flex', visibility: heating ? 'visible' : 'hidden' }}><FlameIcon size={14} still={!heating} /></div>
          {roomName}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 10, whiteSpace: 'nowrap', fontFamily: 'Pretendard', fontSize: 21, fontWeight: 500, letterSpacing: '0.02em', color: QA.dim, marginBottom: 4 }}>
          <span style={{ display: 'flex', alignItems: 'center', gap: 4 }}><ThermoIcon color={QA.dim} />{currentTemp.toFixed(1)}°</span>
          {humidity != null && <span style={{ display: 'flex', alignItems: 'center', gap: 4 }}><DropIcon color={QA.dim} />{Math.round(humidity)}%</span>}
        </div>
        {/* fixed-height slot: ON/OFF swaps only this content, nothing else moves */}
        <div style={{ height: 66, marginTop: 2, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          {state.on ? (
            <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'center', fontFamily: 'Inter Tight', fontSize: 64, fontWeight: 300, lineHeight: 1, color: QA.fg, letterSpacing: '-0.04em', whiteSpace: 'nowrap' }}>
              {target.toFixed(1)}<span style={{ fontSize: 28, color: QA.dim, marginLeft: 4, lineHeight: 1, alignSelf: 'flex-start' }}>°</span>
            </div>
          ) : (
            <div style={{ fontFamily: 'Inter Tight', fontSize: 60, fontWeight: 300, lineHeight: 1, color: QA.dim, letterSpacing: '-0.04em' }}>
              OFF
            </div>
          )}
        </div>
      </div>
      {summary && <BoilerBadge burning={summary.some(r => r.on && r.burning)} />}
      {summary && <RoomSummary rooms={summary} activeName={roomName} />}
    </div>
  );
}

// ─── SETTINGS: focus-centered list ───────────────────────────
function ScreenSettings({ items, selected, settings }) {
  const itemH = 50;
  const fmt = (id) => {
    if (id === 'room') return settings.roomName;
    if (id === 'brightness') return `${settings.brightness}%`;
    if (id === 'screenTime') return settings.screenTime >= 60 ? `${settings.screenTime/60}분` : `${settings.screenTime}초`;
    return '';
  };
  return (
    <div style={{ position: 'absolute', inset: 0, overflow: 'hidden' }}>
      <div style={{
        position: 'absolute', top: 36, left: 0, right: 0,
        textAlign: 'center', fontFamily: 'Pretendard', fontSize: 18, fontWeight: 500,
        letterSpacing: '0.18em', color: QA.dim,
      }}>설정</div>

      <div style={{
        position: 'absolute', top: '50%', left: 0, right: 0,
        transform: `translateY(${-itemH * selected}px)`,
        transition: 'transform .3s cubic-bezier(.2,.8,.2,1)',
      }}>
        {items.map((it, i) => {
          const dist = Math.abs(i - selected);
          const active = dist === 0;
          const scale = active ? 1 : (dist === 1 ? 0.74 : 0.55);
          const opacity = active ? 1 : (dist === 1 ? 0.45 : 0.18);
          const val = fmt(it.id);
          return (
            <div key={it.id} style={{
              position: 'absolute', left: 0, right: 0,
              top: itemH * i - itemH / 2,
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
              padding: '0 30px',
              opacity, transform: `scale(${scale})`, transformOrigin: 'center',
              transition: 'all .3s cubic-bezier(.2,.8,.2,1)',
              whiteSpace: 'nowrap',
            }}>
              <span style={{ fontFamily: 'Pretendard', fontSize: 23, fontWeight: active ? 600 : 400, color: QA.fg }}>{it.ko}</span>
              {val && (
                <span style={{ fontFamily: 'JetBrains Mono', fontSize: 17, color: active ? BOILER_ACCENT : QA.dim }}>{val}</span>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}

// ─── SETTINGS: room edit (focus-centered list) ───
function ScreenRoomEdit({ selected }) {
  const itemH = 48;
  return (
    <div style={{ position: 'absolute', inset: 0, overflow: 'hidden' }}>
      <div style={{
        position: 'absolute', top: 36, left: 0, right: 0,
        textAlign: 'center', fontFamily: 'Pretendard', fontSize: 18, fontWeight: 500,
        letterSpacing: '0.18em', color: QA.dim,
      }}>방 선택</div>
      <div style={{
        position: 'absolute', top: '50%', left: 0, right: 0,
        transform: `translateY(${-itemH * selected}px)`,
        transition: 'transform .3s cubic-bezier(.2,.8,.2,1)',
      }}>
        {ROOMS.map((name, i) => {
          const dist = Math.abs(i - selected);
          const active = dist === 0;
          const scale = active ? 1 : (dist === 1 ? 0.74 : 0.55);
          const opacity = active ? 1 : (dist === 1 ? 0.45 : 0.18);
          return (
            <div key={name} style={{
              position: 'absolute', left: 0, right: 0,
              top: itemH * i - itemH / 2,
              textAlign: 'center',
              opacity, transform: `scale(${scale})`, transformOrigin: 'center',
              transition: 'all .3s cubic-bezier(.2,.8,.2,1)',
              fontFamily: 'Pretendard', fontSize: 26, fontWeight: active ? 600 : 400,
              color: active ? BOILER_ACCENT : QA.fg, whiteSpace: 'nowrap',
            }}>{name}</div>
          );
        })}
      </div>
      <div style={{
        position: 'absolute', bottom: 40, left: 0, right: 0, textAlign: 'center',
        fontFamily: 'Pretendard', fontSize: 17, fontWeight: 500, color: QA.dim, letterSpacing: '0.04em',
      }}>돌려 선택 · 눌러 확인</div>
    </div>
  );
}

// ─── SETTINGS: brightness edit ───────────────────────────────
function ScreenBrightness({ value }) {
  const startDeg = -130, endDeg = 130;
  const t = (value - 10) / 90;
  return (
    <div style={{ position: 'absolute', inset: 0 }}>
      <svg viewBox={`0 0 ${VB} ${VB}`} style={{ position: 'absolute', inset: 0 }}>
        <path d={arcPath(CX, CY, 100, startDeg, endDeg)} stroke={QA.bg2} strokeWidth="7" fill="none" />
        <path d={arcPath(CX, CY, 100, startDeg, startDeg + (endDeg - startDeg) * t)} stroke={BOILER_ACCENT} strokeWidth="8" fill="none" strokeLinecap="round" />
      </svg>
      <div style={{ position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ fontFamily: 'Pretendard', fontSize: 20, fontWeight: 500, letterSpacing: '0.12em', color: QA.dim, marginBottom: 6 }}>
          화면밝기
        </div>
        <div style={{ fontFamily: 'Inter Tight', fontSize: 72, fontWeight: 300, lineHeight: 1, color: QA.fg, letterSpacing: '-0.04em' }}>
          {value}<span style={{ fontSize: 24, color: QA.dim, marginLeft: 3 }}>%</span>
        </div>
        <div style={{ fontFamily: 'Pretendard', fontSize: 13, fontWeight: 500, letterSpacing: '0.04em', color: QA.dim, marginTop: 10 }}>
          돌려 조절 · 눌러 확인
        </div>
      </div>
    </div>
  );
}

// ─── SETTINGS: screen-time edit ──────────────────────────────
function ScreenScreenTime({ value }) {
  const steps = SCREEN_TIME_STEPS;
  const idx = Math.max(0, steps.indexOf(value));
  const startDeg = -130, endDeg = 130;
  const t = idx / (steps.length - 1);
  const label = value >= 60 ? `${value / 60}분` : `${value}초`;
  return (
    <div style={{ position: 'absolute', inset: 0 }}>
      <svg viewBox={`0 0 ${VB} ${VB}`} style={{ position: 'absolute', inset: 0 }}>
        <path d={arcPath(CX, CY, 100, startDeg, endDeg)} stroke={QA.bg2} strokeWidth="7" fill="none" />
        <path d={arcPath(CX, CY, 100, startDeg, startDeg + (endDeg - startDeg) * t)} stroke={BOILER_ACCENT} strokeWidth="8" fill="none" strokeLinecap="round" />
      </svg>
      <div style={{ position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ fontFamily: 'Pretendard', fontSize: 20, fontWeight: 500, letterSpacing: '0.12em', color: QA.dim, marginBottom: 6 }}>
          화면시간
        </div>
        <div style={{ fontFamily: 'Inter Tight', fontSize: 64, fontWeight: 300, lineHeight: 1, color: QA.fg, letterSpacing: '-0.04em' }}>
          {label}
        </div>
        <div style={{ fontFamily: 'Pretendard', fontSize: 13, fontWeight: 500, letterSpacing: '0.04em', color: QA.dim, marginTop: 10 }}>
          돌려 조절 · 눌러 확인
        </div>
      </div>
    </div>
  );
}

// ─── press overlay ───────────────────────────────────────────
function PressOverlay({ accent, label }) {
  return (
    <div style={{
      position: 'absolute', inset: 0, pointerEvents: 'none', zIndex: 7,
      display: 'flex', alignItems: 'center', justifyContent: 'center',
      background: `radial-gradient(circle at 50% 50%, ${accent}40, transparent 65%)`,
      animation: 'qa-press 0.6s ease-out forwards',
    }}>
      <div style={{
        fontFamily: 'Instrument Serif', fontStyle: 'italic', fontSize: 52,
        color: accent,
      }}>{label}</div>
    </div>
  );
}

// ─── App ──────────────────────────────────────────────────────
const SETTINGS_ITEMS = [
  { id: 'room',       ko: '방선택' },
  { id: 'brightness', ko: '화면밝기' },
  { id: 'screenTime', ko: '화면시간' },
  { id: 'back',       ko: '이전' },
];

function App() {
  const [tweaks, setTweaks] = window.useTweaks(/*EDITMODE-BEGIN*/{
    "wifi": 3,
    "roomName": "거실",
    "currentTemp": 21.4,
    "initialOn": true,
    "longPressMs": 500,
    "humidity": 45,
    "showHumidity": true,
    "burnLiving": true,
    "burnBig": false,
    "burnSmall": false,
    "burnWork": false
  }/*EDITMODE-END*/);
  const BURN_KEYS = { '거실': 'burnLiving', '큰방': 'burnBig', '작은방': 'burnSmall', '일하는방': 'burnWork' };
  const burningOf = (r) => !!tweaks[BURN_KEYS[r]];

  // per-room boiler state (거실 unit can control every room)
  const [roomStates, setRoomStates] = useState(() => {
    const o = {};
    ROOMS.forEach(r => { o[r] = { target: 22.0, on: r === tweaks.roomName ? !!tweaks.initialOn : false }; });
    return o;
  });
  // mode: 'main' | 'settings' | 'edit-room' | 'edit-brightness' | 'edit-screenTime'
  const [mode, setMode] = useState('main');
  const [settingsSel, setSettingsSel] = useState(0);
  const [roomIdx, setRoomIdx] = useState(() => {
    const i = ROOMS.indexOf(tweaks.roomName);
    return i >= 0 ? i : 0;
  });
  const [roomEditIdx, setRoomEditIdx] = useState(0);
  // room currently being controlled (only differs from roomIdx on the 거실 unit)
  const [activeIdx, setActiveIdx] = useState(roomIdx);
  const [roomBtnPressed, setRoomBtnPressed] = useState(false);
  const isMaster = ROOMS[roomIdx] === '거실';
  const activeName = ROOMS[activeIdx];
  const state = roomStates[activeName];
  const setState = (fn) => setRoomStates(all => ({ ...all, [activeName]: fn(all[activeName]) }));
  const currentTemps = { '거실': Number(tweaks.currentTemp), '큰방': 19.8, '작은방': 22.1, '일하는방': 20.6 };
  const humidities = { '거실': Number(tweaks.humidity ?? 45), '큰방': 52, '작은방': 47, '일하는방': 38 };

  const onRoomSwitch = () => {
    if (!isMaster || mode !== 'main') return;
    const next = (activeIdx + 1) % ROOMS.length;
    setActiveIdx(next);
    flash(ROOMS[next], SETTING_ACCENT);
  };
  const [settings, setSettings] = useState({ brightness: 70, screenTime: 30 });
  const [pressFx, setPressFx] = useState(null);
  const [btnPressed, setBtnPressed] = useState(false);

  const TEST_MODE = new URLSearchParams(location.search).has('test');
  const flash = (label, accent = BOILER_ACCENT) => {
    if (TEST_MODE) return;
    setPressFx({ accent, label, key: Date.now() });
    setTimeout(() => setPressFx(null), 600);
  };

  // ─── encoder turn ──────────────────────────────────────────
  const encoderTick = useCallback((delta) => {
    if (mode === 'main') {
      setState(cur => {
        if (!cur.on) return cur;
        let nv = cur.target + delta * 0.5;
        nv = Math.max(15, Math.min(30, nv));
        return { ...cur, target: nv };
      });
    } else if (mode === 'settings') {
      setSettingsSel(s => (s + (delta > 0 ? 1 : -1) + SETTINGS_ITEMS.length) % SETTINGS_ITEMS.length);
    } else if (mode === 'edit-room') {
      setRoomEditIdx(i => (i + (delta > 0 ? 1 : -1) + ROOMS.length) % ROOMS.length);
    } else if (mode === 'edit-brightness') {
      setSettings(s => ({ ...s, brightness: Math.max(10, Math.min(100, s.brightness + (delta > 0 ? 5 : -5))) }));
    } else if (mode === 'edit-screenTime') {
      setSettings(s => {
        const i = SCREEN_TIME_STEPS.indexOf(s.screenTime);
        const ni = Math.max(0, Math.min(SCREEN_TIME_STEPS.length - 1, (i < 0 ? 2 : i) + (delta > 0 ? 1 : -1)));
        return { ...s, screenTime: SCREEN_TIME_STEPS[ni] };
      });
    }
  }, [mode, activeName]);

  // ─── short press ───────────────────────────────────────────
  const onShortPress = () => {
    if (mode === 'main') {
      setState(cur => {
        const newOn = !cur.on;
        flash(newOn ? 'ON' : 'OFF');
        return { ...cur, on: newOn };
      });
    } else if (mode === 'settings') {
      const item = SETTINGS_ITEMS[settingsSel];
      if (item.id === 'back') {
        setMode('main');
        flash('이전', SETTING_ACCENT);
      } else if (item.id === 'room') {
        setRoomEditIdx(roomIdx);
        setMode('edit-room');
      } else if (item.id === 'brightness') {
        setMode('edit-brightness');
      } else if (item.id === 'screenTime') {
        setMode('edit-screenTime');
      }
    } else if (mode === 'edit-room') {
      setRoomIdx(roomEditIdx);
      setActiveIdx(roomEditIdx);
      setMode('settings');
      flash(ROOMS[roomEditIdx], SETTING_ACCENT);
    } else if (mode === 'edit-brightness' || mode === 'edit-screenTime') {
      setMode('settings');
      flash('확인', SETTING_ACCENT);
    }
  };

  // ─── long press ────────────────────────────────────────────
  const onLongPress = () => {
    if (mode === 'main') {
      setSettingsSel(0);
      setMode('settings');
      flash('설정', SETTING_ACCENT);
    }
  };

  // physical button (mouse, keyboard, touch)
  const btnTimer = useRef(null);
  const btnFiredLong = useRef(false);
  const onBtnDown = (e) => {
    if (e) e.preventDefault();
    setBtnPressed(true);
    btnFiredLong.current = false;
    btnTimer.current = setTimeout(() => {
      btnFiredLong.current = true;
      onLongPress();
    }, tweaks.longPressMs);
  };
  const onBtnUp = (e) => {
    if (e) e.preventDefault();
    setBtnPressed(false);
    if (btnTimer.current) { clearTimeout(btnTimer.current); btnTimer.current = null; }
    if (!btnFiredLong.current) onShortPress();
  };
  const onBtnCancel = () => {
    setBtnPressed(false);
    if (btnTimer.current) { clearTimeout(btnTimer.current); btnTimer.current = null; }
    btnFiredLong.current = true; // don't fire short
  };

  // keep latest handlers in a ref so the keydown listener never goes stale
  const handlersRef = useRef({ onShortPress, onBtnDown, onBtnUp, encoderTick, onRoomSwitch, onLongPress });
  handlersRef.current = { onShortPress, onBtnDown, onBtnUp, encoderTick, onRoomSwitch, onLongPress };
  const snapRef = useRef(null);
  snapRef.current = {
    mode, installedRoom: ROOMS[roomIdx], activeRoom: activeName, isMaster,
    settingsSel, roomEditSel: ROOMS[roomEditIdx],
    brightness: settings.brightness, screenTime: settings.screenTime,
    rooms: Object.fromEntries(ROOMS.map(r => [r, { on: roomStates[r].on, target: roomStates[r].target, current: currentTemps[r], humidity: humidities[r], burning: burningOf(r) }])),
  };
  // test API for scripted replay (Puppeteer): enc+ enc- short long room, {burn:[room,bool]}
  useEffect(() => {
    window.__thermo = {
      dispatch: (e) => {
        const h = handlersRef.current;
        if (e === 'enc+') h.encoderTick(+1);
        else if (e === 'enc-') h.encoderTick(-1);
        else if (e === 'short') h.onShortPress();
        else if (e === 'long') h.onLongPress();
        else if (e === 'room') h.onRoomSwitch();
        else if (e && e.burn) setTweaks(BURN_KEYS[e.burn[0]], !!e.burn[1]);
      },
      getState: () => JSON.parse(JSON.stringify(snapRef.current)),
    };
    window.__THERMO_READY = true;
  }, []);

  // keyboard
  useEffect(() => {
    const isSpace = (e) => e.key === ' ' || e.key === 'Spacebar' || e.code === 'Space';
    const onKeyDown = (e) => {
      if (isSpace(e)) {
        if (e.repeat) return;
        e.preventDefault();
        handlersRef.current.onBtnDown();
      } else if (e.key === 'ArrowRight' || e.key === '=' || e.key === '+') {
        e.preventDefault(); handlersRef.current.encoderTick(+1);
      } else if (e.key === 'ArrowLeft' || e.key === '-' || e.key === '_') {
        e.preventDefault(); handlersRef.current.encoderTick(-1);
      } else if (e.key === 'Enter') {
        e.preventDefault(); handlersRef.current.onShortPress();
      } else if (e.key === 'r' || e.key === 'R') {
        e.preventDefault(); handlersRef.current.onRoomSwitch();
      }
    };
    const onKeyUp = (e) => {
      if (isSpace(e)) { e.preventDefault(); handlersRef.current.onBtnUp(); }
    };
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
    };
  }, []);

  // ─── encoder ring drag rotation → ticks ────────────────────
  const knobRef = useRef(null);
  const dragRef = useRef({ active: false, lastAngle: 0, accum: 0 });
  const onPointerDown = (e) => {
    if (!knobRef.current) return;
    knobRef.current.setPointerCapture(e.pointerId);
    const rect = knobRef.current.getBoundingClientRect();
    const cx = rect.left + rect.width / 2, cy = rect.top + rect.height / 2;
    const ang = Math.atan2(e.clientY - cy, e.clientX - cx);
    dragRef.current = { active: true, lastAngle: ang, accum: 0 };
  };
  const onPointerMove = (e) => {
    const d = dragRef.current;
    if (!d.active) return;
    const rect = knobRef.current.getBoundingClientRect();
    const cx = rect.left + rect.width / 2, cy = rect.top + rect.height / 2;
    const ang = Math.atan2(e.clientY - cy, e.clientX - cx);
    let delta = ang - d.lastAngle;
    while (delta > Math.PI)  delta -= 2 * Math.PI;
    while (delta < -Math.PI) delta += 2 * Math.PI;
    d.lastAngle = ang;
    d.accum += delta;
    const stepRad = Math.PI / 15;
    while (d.accum > stepRad)  { encoderTick(+1); d.accum -= stepRad; }
    while (d.accum < -stepRad) { encoderTick(-1); d.accum += stepRad; }
  };
  const onPointerUp = () => { dragRef.current.active = false; };

  const onWheel = (e) => {
    e.preventDefault();
    encoderTick(e.deltaY > 0 ? -1 : 1);
  };

  const roomName = ROOMS[roomIdx];
  let screen;
  if (mode === 'main') {
    screen = <ScreenBoiler roomName={activeName} currentTemp={currentTemps[activeName]} humidity={tweaks.showHumidity === false || activeName !== ROOMS[roomIdx] ? null : humidities[activeName] /* other rooms arrive over PLC without humidity */} state={state} burning={burningOf(activeName)}
      summary={isMaster ? ROOMS.map(r => ({ name: r, on: roomStates[r].on, burning: burningOf(r) })) : null} />;
  } else if (mode === 'settings') {
    screen = <ScreenSettings items={SETTINGS_ITEMS} selected={settingsSel} settings={{ ...settings, roomName }} />;
  } else if (mode === 'edit-room') {
    screen = <ScreenRoomEdit selected={roomEditIdx} />;
  } else if (mode === 'edit-brightness') {
    screen = <ScreenBrightness value={settings.brightness} />;
  } else if (mode === 'edit-screenTime') {
    screen = <ScreenScreenTime value={settings.screenTime} />;
  }

  // simulated LCD dim based on brightness setting
  const lcdOpacity = TEST_MODE ? 1 : 0.6 + 0.4 * (settings.brightness / 100);

  return (
    <>
      <style>{`
        @keyframes qa-press {
          0%   { opacity: 0; }
          25%  { opacity: 1; }
          100% { opacity: 0; }
        }
      `}</style>

      <div className="header">
        <div className="meta">ROUND REMOTE · 보일러 · 1.28" 240×240 LCD · 인코더 + 버튼 1</div>
        <h1>boiler</h1>
        <p>인코더로 희망온도를 돌리고, 버튼을 짧게 눌러 켜고 끕니다. 길게 누르면 설정으로 들어갑니다.</p>
      </div>

      <div className="stage">
        <div className="knob">
          <div
            className="knob-frame"
            ref={knobRef}
            onPointerDown={onPointerDown}
            onPointerMove={onPointerMove}
            onPointerUp={onPointerUp}
            onPointerCancel={onPointerUp}
            onWheel={onWheel}
          >
            <div className="knob-notch" />
            <div className="screen" id="lcd" style={{ opacity: lcdOpacity }}>
              {screen}
              <StatusBar wifi={tweaks.wifi} />
              {pressFx && <PressOverlay key={pressFx.key} accent={pressFx.accent} label={pressFx.label} />}
            </div>
          </div>

          <div style={{ display: 'flex', gap: 14 }}>
            <div
              className={'pushbtn' + (btnPressed ? ' pressed' : '')}
              onPointerDown={onBtnDown}
              onPointerUp={onBtnUp}
              onPointerCancel={onBtnCancel}
              onPointerLeave={() => { if (btnPressed) onBtnCancel(); }}
            >
              <span className="led" />PUSH
            </div>
            {isMaster && (
              <div
                className={'pushbtn' + (roomBtnPressed ? ' pressed' : '')}
                onPointerDown={(e) => { e.preventDefault(); setRoomBtnPressed(true); }}
                onPointerUp={(e) => { e.preventDefault(); if (roomBtnPressed) onRoomSwitch(); setRoomBtnPressed(false); }}
                onPointerLeave={() => setRoomBtnPressed(false)}
              >
                <span className="led" style={{ background: '#7fc8d4', boxShadow: '0 0 6px rgba(127,200,212,.7)' }} />ROOM
              </div>
            )}
          </div>
        </div>

        <div className="controls">
          <h3>조작 방법</h3>
          <div className="ctrl-grid">
            <div className="ctrl-key">turn</div><div className="ctrl-val">인코더 → 희망온도 ±0.5°</div>
            <div className="ctrl-key">click</div><div className="ctrl-val">짧게 → 전원 ON/OFF</div>
            <div className="ctrl-key">hold</div><div className="ctrl-val">길게 → 설정 진입</div>
            <div className="ctrl-key">set</div><div className="ctrl-val">설정: 회전·짧게 항목 진입/확인 · "이전"으로 나감</div>
            {isMaster && <><div className="ctrl-key">ROOM</div><div className="ctrl-val">거실 기기 전용 · 제어할 방 전환 (R 키)</div></>}
          </div>

          <div className="matrix">
            <div className="label">현재 상태 · {activeName}{activeName !== roomName ? ` (설치: ${roomName})` : ''}</div>
            <div style={{ fontSize: 13, fontFamily: 'Pretendard', lineHeight: 1.7, color: '#333' }}>
              <div><span style={{ color:'#888' }}>현재</span> {currentTemps[activeName].toFixed(1)}°</div>
              <div><span style={{ color:'#888' }}>희망</span> {state.on ? `${state.target.toFixed(1)}°` : 'OFF'}</div>
              <div><span style={{ color:'#888' }}>전원</span> {state.on ? 'ON' : 'OFF'}</div>
              <div><span style={{ color:'#888' }}>연소</span> {state.on && burningOf(activeName) ? '가동 중' : '대기'}</div>
              {isMaster && <div><span style={{ color:'#888' }}>전체</span> {ROOMS.map(r => `${r} ${roomStates[r].on ? (burningOf(r) ? '연소' : 'ON') : 'OFF'}`).join(' · ')}</div>}
              {isMaster && <div><span style={{ color:'#888' }}>보일러</span> {ROOMS.some(r => roomStates[r].on && burningOf(r)) ? '연소 중' : '대기'}</div>}
              <div><span style={{ color:'#888' }}>화면밝기</span> {settings.brightness}%</div>
              <div><span style={{ color:'#888' }}>화면시간</span> {settings.screenTime >= 60 ? `${settings.screenTime/60}분` : `${settings.screenTime}초`}</div>
              <div><span style={{ color:'#888' }}>방</span> {roomName}</div>
            </div>
          </div>

          <div className="footnote">
            방 이름은 펌웨어/설치 시 한 번 지정합니다. 설정에서 화면밝기·화면시간을 조절할 수 있고, 마지막 "이전" 항목으로 빠져나옵니다.
          </div>
        </div>
      </div>

      <window.TweaksPanel title="Tweaks">
        <window.TweakSection title="보일러 신호 (시뮬) · 방별 연소">
          {ROOMS.map(r => (
            <window.TweakToggle key={r} label={`${r} 연소`} value={!!tweaks[BURN_KEYS[r]]} onChange={(v) => setTweaks(BURN_KEYS[r], v)} />
          ))}
        </window.TweakSection>
        <window.TweakSection title="설치 정보">
          <window.TweakSlider label="현재 온도" value={tweaks.currentTemp} onChange={(v) => setTweaks('currentTemp', v)} min={5} max={35} step={0.1} valueLabel={`${Number(tweaks.currentTemp).toFixed(1)}°`} />
        </window.TweakSection>
        <window.TweakSection title="상태 표시">
          <window.TweakToggle label="습도 표시" value={tweaks.showHumidity !== false} onChange={(v) => setTweaks('showHumidity', v)} />
          <window.TweakSlider label="거실 습도" value={tweaks.humidity ?? 45} onChange={(v) => setTweaks('humidity', v)} min={0} max={100} step={1} valueLabel={`${tweaks.humidity ?? 45}%`} />
          <window.TweakSlider label="WiFi 신호" value={tweaks.wifi} onChange={(v) => setTweaks('wifi', v)} min={0} max={3} step={1} valueLabel={`${tweaks.wifi}/3`} />
        </window.TweakSection>
        <window.TweakSection title="버튼">
          <window.TweakSlider label="길게 누름" value={tweaks.longPressMs} onChange={(v) => setTweaks('longPressMs', v)} min={250} max={1500} step={50} valueLabel={`${tweaks.longPressMs}ms`} />
        </window.TweakSection>
      </window.TweaksPanel>
    </>
  );
}

Object.assign(window, {
  StatusBar, FlameIcon, BoilerBadge, RoomSummary,
  ScreenBoiler, ScreenSettings, ScreenRoomEdit, ScreenBrightness, ScreenScreenTime,
  SETTINGS_ITEMS, ROOMS, SCREEN_TIME_STEPS, BOILER_ACCENT, QA, VB,
});

if (document.getElementById('root')) {
  ReactDOM.createRoot(document.getElementById('root')).render(<App />);
}
