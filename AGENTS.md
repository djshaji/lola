# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project Snapshot

- Project: LV2 loader in C++.
- Goal: load LV2 plugins, inspect metadata, and instantiate plugin handles.
- Primary references: [Readme.md](Readme.md), [src/lola.h](src/lola.h), [src/lola.cc](src/lola.cc), and [tools/ttl_parse.py](tools/ttl_parse.py).

## Working Conventions

- Keep changes focused and minimal. Preserve the existing public API unless the task explicitly requests a change.
- C++ work should stay in the header/source split under [src](src) and follow the current style:
  - include guards in headers
  - logging through the macros in [src/log.h](src/log.h)
  - explicit cleanup for resource ownership, especially around plugin lifecycle and `dlopen`/`dlclose`
- The TTL parser helper in [tools/ttl_parse.py](tools/ttl_parse.py) should make small, explicit RDF extraction changes and keep its JSON output shape stable.

## Build and Validation

- Verified commands:
  - `cd src && make` builds `main` and `repro_ratatouille`.
  - `cd src && ./repro_ratatouille` runs the Ratatouille smoke repro.
  - `python3 tests/validate_features.py` validates fixture expectations.
- TTL parser usage:
  - `python3 tools/ttl_parse.py src/Ratatouille.lv2 > src/Ratatouille.lv2/Ratatouille.json`
  - `python3 tools/ttl_parse.py src/dyson_compress-swh.lv2 > src/dyson_compress-swh.lv2/plugin.json`
- The Makefile depends on JACK and GTK development headers being available, so build failures may reflect missing system packages rather than a code issue.
- The parser and fixture validator require `rdflib` in the Python environment.
- This repository does not currently have a broader CI or test harness, so do not assume one exists.

## Important Implementation Notes

- The plugin lifecycle is safety-sensitive: `Plugin` owns shared library loading, instantiation, and cleanup. Prefer RAII-style cleanup and avoid leaks when adding lifecycle logic.
- [src/lola.h](src/lola.h) already declares `Control` and other plugin state, but the implementation is still partial. Keep changes conservative around those areas.
- Fixture data in [tests/fixtures](tests/fixtures) is useful when validating LV2 feature handling and parser behavior.
- Prefer validating feature changes with [tests/validate_features.py](tests/validate_features.py) before touching broader loader behavior.
- Keep generated bundle JSON stable in shape; when changing parser output, update fixtures in [tests/fixtures](tests/fixtures) intentionally.

## Port Wiring Safety

- Treat LV2 port connectivity as mandatory before `run()`: connect every declared plugin port to valid host memory, including output control ports and atom ports.
- Do not leave output control ports unconnected; plugin code may write to those pointers during processing.
- [src/main.cc](src/main.cc) currently exposes one JACK input and one JACK output port. Do not infer a second channel by offsetting a single JACK buffer pointer.
- When plugin stereo ports exist but host side is mono, connect secondary ports to valid fallback buffers until true multi-port JACK wiring is implemented.

## Session Hygiene

- In long debugging chats, compact periodically (for example with `/compact`) after confirming a repro or a fix direction.
- Preserve a short running note of repro command, observed failure, and latest result so follow-up turns stay focused.

## Documentation

- Link to existing docs instead of duplicating them:
  - [Readme.md](Readme.md)
  - [LICENSE](LICENSE)
