AGENTS.md

Build/lint/test
- Build: pio run
- Upload: pio run -t upload
- Monitor: pio run -t monitor (configure speed in platformio.ini)
- Clean: pio run -t clean
- Tests (all): pio test
- Tests (single): pio test -f <test_name>


Code style
- Language: C++ (Arduino framework). Mirror patterns in src/main.cpp and existing sources.
- Imports: angle brackets for Arduino/standard headers (<Arduino.h>, <stdint.h>); quotes for project headers (include/, lib/).
- Include order: C/C++ standard headers first, then Arduino headers, then project headers. Remove unused includes.
- Formatting: 4-space indentation; braces on next line as in repo; ~100 char line length; keep includes grouped and ordered.
- Types: prefer fixed-width integer types from <stdint.h>. Avoid magic literals; use const and constexpr where applicable.
- Naming: UPPER_SNAKE_CASE for macros/log TAGs; lower_snake_case for vars/functions (e.g., read_interval_ms); PascalCase for classes and structs.
- Error handling: check return values; use LOG_ERROR for errors; early-return on failures.
- Concurrency: Use delay() for simple timing; avoid blocking operations.
- Memory: prefer stack/static allocation; be careful with dynamic allocation.
- Build config: platformio.ini is source of truth (env esp32dev, framework arduino, lib_deps as listed).

Repository conventions
- Keep code organized in src/, include/, and lib/ directories.
- Do not commit secrets (WiFi credentials, MQTT passwords). Parameterize via configuration files in SPIFFS.
- Before PRs, build and run all tests for env esp32dev.