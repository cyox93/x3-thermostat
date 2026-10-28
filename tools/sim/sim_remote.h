#pragma once

/* Test commands (docs/ui.md §6.2) on a loopback TCP port: one connection =
   one command = one reply. Polled from the main loop, so every LVGL call
   stays on one thread. */
void sim_remote_init(int port);
void sim_remote_poll(void);          /* accepts at most one connection per call */
void sim_remote_shutdown(void);
