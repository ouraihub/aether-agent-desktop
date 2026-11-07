# Aether — Universal Agent Desktop

A cross-platform desktop workspace for managing and interacting with multiple Agent Runtimes through one application.

## Project status

The repository is in the initial C17 engineering phase.

Implemented and verified on Windows:

- C17/CMake project skeleton.
- `aether_foundation` and `aether_core` module targets.
- Minimal Core lifecycle smoke test.
- Foundation ID and bounded error smoke test.
- CTest integration.
- Source size and C line-width checker.
- Desktop skeleton executable.

Not implemented yet:

- Backend API and request orchestration.
- Frontend and UI framework integration.
- Runtime Adapters.
- Persistence and configuration storage.
- MCP, Skills and Tool Registry.
- Cross-platform runtime verification.

The project task board is the source of implementation progress:

- [Task board](docs/tasks/README.md)
- [Execution log](docs/tasks/EXECUTION_LOG.md)
- [Project overview design](docs/design/README.md)
- [Detailed implementation design](docs/design/detail/README.md)
- [Code placement plan](docs/design/CODE_LAYOUT.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Interface contracts](docs/INTERFACE_CONTRACTS.md)
- [C17 engineering standards](docs/CODING_STANDARDS.md)

## Current Windows build

Verified environment:

- Windows.
- CMake 4.2.3.
- Clang 22.1.4, `x86_64-w64-windows-gnu` target.
- Ninja 1.13.2.
- Python 3.13.5 for source checks.

Configure, build, test and run the current skeleton:

```text
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
./build/default/aether_desktop.exe
python tools/check_code.py
```

The current preset is a Windows development entry point. macOS and Linux are not yet verified.

## Repository layout

```text
apps/desktop/                 Desktop composition root
include/aether/foundation/   Foundation public headers
include/aether/core/         Core public headers
src/foundation/               Foundation implementation
src/core/                     Core implementation
tests/foundation/             Foundation tests
tests/core/                   Core tests
tools/                        Development checks
docs/design/                  Project overview and layered design
docs/tasks/                   Implementation task board and evidence
```

New code must follow [the code placement plan](docs/design/CODE_LAYOUT.md). Do not create empty future modules or place unrelated implementation in a shared `manager.c`/`common.c` file.

## Engineering constraints

- Self-owned business code uses ISO C17.
- Frontend, Backend, Core, Adapter, Infrastructure and Platform boundaries are explicit.
- Runtime-specific behavior belongs in independent Adapters.
- Public interfaces define ownership, lifecycle, errors, threading and cancellation semantics.
- Hand-written code files are limited to 500 physical lines; files over 300 lines require responsibility review.
- C source targets 80 columns and has a normal 100-column limit.
- Unverified behavior must not be reported as implemented.

See [AGENTS.md](AGENTS.md) before changing the project.
