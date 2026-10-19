# Firmware

- Target: ESP32-S3, **native ESP-IDF v5.3+** (no Arduino). Drivers: `i2c_master`, `adc_oneshot`, LEDC, GPIO, USB-Serial-JTAG, NimBLE host, ESP-NOW; managed components `espressif/led_strip`, `joltwallet/littlefs`, `u8g2`.
- Hardware access goes through `main/hal/platform.h`; update `tests/host_compile/idf_stubs/` (signatures copied from the real headers) when you call a new IDF API.
- The control loop is 100 Hz on core 1 and must never block: no `vTaskDelay`/`delayMs` inside the loop body (the loop paces itself with `vTaskDelayUntil`), no busy-waits longer than ~100 µs, no blocking I/O. Slow work (OLED) runs in its own task on core 0.
- Shared I²C goes through `hal::I2cLock`.
- New syscall: add to `spec/opcodes.json` → regenerate → implement in `RobotHost::syscall` → mark `stage: "v1"` only when implemented (the capability bitmap comes from it).
- Never weaken safety: cliff guard, A+B / long-press-touch stop, speed cap, validator, watchdog.
