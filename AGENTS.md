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
  - `cd src && make` builds the demo host.
  - `python3 tests/validate_features.py` validates the fixture expectations.
- The Makefile depends on JACK and GTK development headers being available, so build failures may reflect missing system packages rather than a code issue.
- This repository does not currently have a broader CI or test harness, so do not assume one exists.

## Important Implementation Notes

- The plugin lifecycle is safety-sensitive: `Plugin` owns shared library loading, instantiation, and cleanup. Prefer RAII-style cleanup and avoid leaks when adding lifecycle logic.
- [src/lola.h](src/lola.h) already declares `Control` and other plugin state, but the implementation is still partial. Keep changes conservative around those areas.
- Fixture data in [tests/fixtures](tests/fixtures) is useful when validating LV2 feature handling and parser behavior.

## Documentation

- Link to existing docs instead of duplicating them:
  - [Readme.md](Readme.md)
  - [LICENSE](LICENSE)
