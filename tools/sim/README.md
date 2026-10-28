# PC 시뮬레이터

`components/ui` · `components/state`를 macOS SDL2로 빌드한다 (`docs/ui.md` §6.1).

    brew install sdl2 cmake ninja
    # LVGL 소스는 managed_components/lvgl__lvgl — ESP 빌드를 한 번 먼저 한다
    cmake -S tools/sim -B tools/sim/build -G Ninja && cmake --build tools/sim/build
    tools/sim/build/sim

TCP 9999로 시험 명령(`docs/ui.md` §6.2)을 받는다: `echo "ACTIVE 큰방" | nc 127.0.0.1 9999`.
`ctest --test-dir tools/sim/build`는 `app_state` 단위 시험이다.
