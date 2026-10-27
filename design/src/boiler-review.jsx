// Static UI review gallery — renders every screen state in print-friendly pages.
const { useState } = React;

const PAGES = [
  {
    num: '01',
    title: '메인 · 켜짐',
    desc: '보일러가 켜진 기본 상태. 화면 상단에 현재 선택된 방과 측정 온도·습도, 중앙에 큰 글씨로 희망온도를 표시합니다. 보일러가 실제 연소 중일 때만 "희망온도" 옆에 불꽃 아이콘이 표시됩니다. 인코더를 돌리면 0.5°C 단위로 15–30°C 사이에서 조절됩니다.',
    flow: '인코더 좌/우 → 희망온도 -0.5° / +0.5° · 버튼 짧게 → OFF · 버튼 길게 (≥500ms) → 설정',
    state: { kind: 'main', state: { on: true, target: 22.0 }, currentTemp: 21.4, humidity: 45, roomName: '큰방', burning: true },
  },
  {
    num: '02',
    title: '메인 · 꺼짐',
    desc: '전원이 꺼진 상태. 큰 OFF 글씨와 함께 "버튼을 눌러 켜기" 힌트를 표시합니다. 인코더 회전은 무시되며, 버튼 짧게 눌러서 다시 켤 수 있습니다.',
    flow: '버튼 짧게 → ON · 버튼 길게 → 설정',
    state: { kind: 'main', state: { on: false, target: 22.0 }, currentTemp: 21.4, humidity: 45, roomName: '큰방' },
  },
  {
    num: '02-M',
    title: '메인 · 거실 기기',
    desc: '거실에 설치된 기기는 모든 방을 제어합니다. 상단 "보일러" 표시는 어느 방이든 연소 중이면 주황 불꽃으로 켜지고, 하단 거·큰·일·노는 방별 상태입니다. 켜진 방은 밝은 회색, 연소 중인 방은 글자 앞에 불꽃, 지금 제어 중인 방은 네모 박스로 표시합니다.',
    flow: 'ROOM 버튼 → 제어할 방 전환 (거실 → 큰방 → 일하는방 → 노는방) · 나머지 조작은 메인과 같음',
    state: { kind: 'main', state: { on: true, target: 22.0 }, currentTemp: 21.4, humidity: 45, roomName: '거실', burning: true,
      summary: [{ name: '거실', on: true, burning: true }, { name: '큰방', on: false, burning: false }, { name: '일하는방', on: true, burning: false }, { name: '노는방', on: false, burning: false }] },
  },
  {
    num: '03',
    title: '설정 · 목록',
    desc: '메인에서 버튼을 길게 누르면 진입. 4개의 항목을 회전식 리스트로 표시합니다. 현재 선택된 항목은 가운데에 강조되고, 우측에 현재 값이 함께 표시됩니다.',
    flow: '인코더 → 항목 이동 · 버튼 짧게 → 항목 진입 / "이전"에서 메인 복귀',
    state: { kind: 'settings', selected: 0, settings: { brightness: 70, screenTime: 30, roomName: '거실' } },
    pageBreak: true,
  },
  {
    num: '04',
    title: '설정 · 방선택',
    desc: '4개의 방 중 하나를 선택. 현재 활성 방이 가운데 강조되고, 위·아래로 미리보기가 함께 표시됩니다. 변경 후 짧게 눌러 확정합니다.',
    flow: '인코더 → 방 이동 · 버튼 짧게 → 확정 후 설정 목록으로 복귀',
    state: { kind: 'room', selected: 2 }, // 일하는방
  },
  {
    num: '05',
    title: '설정 · 화면밝기',
    desc: '10–100% 사이를 5% 단위로 조절. 상단의 원호 게이지가 현재 값을 시각화하고, 가운데에 큰 숫자로 정확한 값을 표시합니다. 실시간으로 LCD 밝기에 반영됩니다.',
    flow: '인코더 → ±5% · 버튼 짧게 → 확정 후 설정 목록으로 복귀',
    state: { kind: 'brightness', value: 70 },
    pageBreak: true,
  },
  {
    num: '06',
    title: '설정 · 화면시간',
    desc: '비활성 시 화면이 꺼지는 시간. 10초·20초·30초·1분·2분·5분의 6단계로 제공됩니다. 원호 게이지 옆 눈금이 6단계 위치를 보여줍니다.',
    flow: '인코더 → 단계 이동 · 버튼 짧게 → 확정 후 설정 목록으로 복귀',
    state: { kind: 'screenTime', value: 30 },
  },
];

function Frame({ children }) {
  return (
    <div className="knob-frame">
      <div className="knob-notch" />
      <div className="screen" style={{ position: 'relative' }}>
        {children}
      </div>
    </div>
  );
}

function renderScreen(s) {
  if (s.kind === 'main') return <window.ScreenBoiler roomName={s.roomName} currentTemp={s.currentTemp} humidity={s.humidity} state={s.state} burning={s.burning} summary={s.summary || null} />;
  if (s.kind === 'settings') return <window.ScreenSettings items={window.SETTINGS_ITEMS} selected={s.selected} settings={s.settings} />;
  if (s.kind === 'room') return <window.ScreenRoomEdit selected={s.selected} />;
  if (s.kind === 'brightness') return <window.ScreenBrightness value={s.value} />;
  if (s.kind === 'screenTime') return <window.ScreenScreenTime value={s.value} />;
  return null;
}

function Page({ page }) {
  return (
    <section className={`page${page.pageBreak ? ' break' : ''}`}>
      <div className="row">
        <div className="frame-col">
          <Frame>{renderScreen(page.state)}</Frame>
          <div className="caption">{page.num} · {page.title}</div>
        </div>
        <div className="text-col">
          <div className="num">{page.num}</div>
          <h2>{page.title}</h2>
          <p className="desc">{page.desc}</p>
          <div className="flow">
            <div className="lbl">interaction</div>
            <div>{page.flow}</div>
          </div>
        </div>
      </div>
    </section>
  );
}

function Review() {
  return (
    <>
      {PAGES.map(p => <Page key={p.num} page={p} />)}
      <section className="page">
        <div className="num">07 · 상태 다이어그램</div>
        <h2>전체 플로우</h2>
        <div style={{ fontSize: 12, lineHeight: 1.8, color: '#333', fontFamily: 'JetBrains Mono, monospace', whiteSpace: 'pre', background: '#fff', border: '1px solid #e0ddd6', borderRadius: 10, padding: '16px 20px' }}>
{`  ┌─────────────┐  버튼 길게(≥500ms)  ┌──────────────┐
  │   메인 ON   │ ──────────────────▶ │   설정 목록   │
  │   /  OFF    │ ◀──────────────── │  ("이전" 선택) │
  └─────────────┘     버튼 짧게        └──────┬───────┘
       ▲                                      │ 버튼 짧게
       │ 버튼 짧게 (토글)                       ▼
       │                          ┌──────────────────┐
       │                          │ 방선택 / 밝기 /    │
       │                          │ 화면시간 (편집)    │
       │                          └──────┬───────────┘
       │                                  │ 버튼 짧게 (확정)
       └──── 인코더: 희망온도 ±0.5°  ─────┘`}
        </div>
      </section>
    </>
  );
}

(async () => {
  try { await document.fonts.ready; } catch (e) {}
  // wait for boiler-app.jsx to finish (exposes window.ScreenBoiler etc)
  await new Promise(r => {
    const tick = () => (window.ScreenBoiler && window.ScreenSettings) ? r() : requestAnimationFrame(tick);
    tick();
  });
  ReactDOM.createRoot(document.getElementById('review-root')).render(<Review />);
  // wait two animation frames + buffer, then open print dialog
  await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
  await new Promise(r => setTimeout(r, 1000));
  window.print();
})();
