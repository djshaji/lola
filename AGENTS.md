# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project Snapshot

- Project: LV2 Loader (C++)
- Goal: Load LV2 plugins, inspect metadata, and instantiate plugin handles
- Primary reference: [Readme.md](Readme.md)

## Build, Run, and Test Reality

- `src/Makefile` exists but is currently empty.
- There is no verified build command, no CI config, and no test harness in this repo.
- Do not assume `make`, `cmake`, or tests exist.
- If a task requires building or testing, either:
  - implement the requested build/test setup as part of that task, or
  - state clearly that no build/test command is currently available.

## Code Map

- `src/lola.h`: Core model types
  - `PortType` enum
  - `Control` class (currently only fields)
  - `Plugin` class declaration and constructor signature
- `src/lola.cc`: `Plugin` constructor implementation
  - `dlopen` shared object
  - `dlsym("lv2_descriptor")`
  - instantiate LV2 plugin handle
- `src/log.h`: Linux-only logging macros (`LOGE`, `LOGI`, `LOGD`, `HERE`)
- `src/main.cc`: Minimal scaffold executable

## Conventions to Follow

- Language/style follows existing C++ patterns:
  - Header/source split with `.h` and `.cc`
  - Include guards in headers
  - Internal logging through `LOG*` macros
- Keep changes minimal and targeted. Avoid broad refactors unless requested.
- Preserve current public API shape unless the task explicitly asks for API changes.

## Safety and Correctness Priorities

- Treat plugin lifecycle/resource handling as high risk:
  - `dlopen`/`dlclose`
  - LV2 instantiate/cleanup pairing
- Prefer explicit error handling over silent failures.
- When adding lifecycle logic, favor RAII-style cleanup to prevent leaks.

## Common Pitfalls in This Repo

- `Plugin` currently has constructor-side acquisition but no visible destructor cleanup.
- `Control` is declared but functionally incomplete.
- Build/test instructions in docs are limited; avoid inventing commands as facts.

## Documentation Linking Rule

- Link to existing docs instead of duplicating them:
  - [Readme.md](Readme.md)
  - [LICENSE](LICENSE)
