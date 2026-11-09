# Aether — Universal Agent Desktop

A cross-platform desktop workspace for managing and interacting with multiple Agent Runtimes through one application.

## Project status

The repository is in the initial C17 engineering phase.

Implemented and verified on Windows:

- C17/CMake project skeleton.
- `aether_foundation`, `aether_core`, `aether_backend` and `aether_platform` targets.
- Core and Backend lifecycle smoke tests.
- Synchronous bounded Backend request records: submit, snapshot query,
  one terminal result, explicit release and stop-time local cancellation.
- Foundation ID and bounded error smoke test.
- Core Runtime Registry: typed Provider/Runtime identities, generation checks,
  capability snapshots and reference-safe two-phase unregister (no Runtime IO).
- Platform monotonic time, Unix-epoch wall time, owned UTF-8 paths and OS temp/
  user-data directory queries; real Chinese/non-BMP directory regression tests.
- CTest integration.
- Source size and C line-width checker.
- Desktop skeleton executable.

Not implemented yet:

- Business Command/Query/Event orchestration, async scheduling and tracing.
- Frontend and UI framework integration.
- Runtime Adapters.
- Persistence and configuration storage.
- MCP, Skills and Tool Registry.
- Full cross-platform runtime verification (Platform smoke also ran on WSL Ubuntu;
  this does not validate a Linux desktop or any macOS behavior).

The project task board is the source of implementation progress:

- [Task board](docs/tasks/README.md)
- [Execution log](docs/tasks/EXECUTION_LOG.md)
- [Project overview design](docs/design/README.md)
- [Detailed implementation design](docs/design/detail/README.md)
- [Code placement plan](docs/design/CODE_LAYOUT.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Interface contracts](docs/INTERFACE_CONTRACTS.md)
- [C17 engineering standards](docs/CODING_STANDARDS.md)

### Credential storage decision

Persistent credentials follow the explicitly accepted
[ADR-0012](docs/adr/0012-sqlite-credentials.md): OMP-style JSON in ordinary,
**unencrypted SQLite**, with SQLite 3.54.1 and yyjson 0.13.0 as the fixed baseline.
The database, WAL/journal and backups may contain secrets. Deletion is not
forensic erasure; OS file permissions do not isolate processes running as the
same user. Ordinary configuration retains `secret_ref`, not secret values.
Temporary keys are not persisted. Storage failures reject dependent operations
without empty-value, stale-cache or environment fallback. This decision does
not permit secret logging, UI reveal/copy, or automatic reading of external
Pi/OMP credentials, and does not introduce an encryption fallback or OS secret
backend. ENG-07-C tracks the credential implementation and acceptance; the
parent ENG-07 network/HTTP work and DATA configuration integration remain
separate, unfinished acceptance scopes.

## Windows environment and build

The commands below use Windows `cmd.exe`. Run them from the repository root.

### Install prerequisites

The repository does not vendor the compiler or build tools. Install these once:

```bat
scoop install cmake ninja uv
```

`uv` is the only Python entry required for the source checker. Do not install
a separate Python distribution just for this repository. Verify the tools:

```bat
cmake --version
ninja --version
uv --version
uv run --no-project python --version
```

### Portable MSVC setup

The repository includes scripts adapted from the shared portable-MSVC workflow
used by adjacent Windows projects. They download and initialize MSVC only when
the configured toolchain is missing. The downloader obtains Microsoft's
components and the Microsoft license terms apply; it is not a compiler bundled
in this repository.

Run once from the repository root on a new Windows machine:

```bat
scripts\setup-msvc.bat
```

The script downloads about 455 MB through `uv` and Windows `curl`, installing
MSVC 14.44 and Windows SDK 26100 x64 under:

```text
D:\Applications\msvc-portable\msvc
```

Set `PORTABLE_MSVC_ROOT` before running the script to choose another location.
After installation, the toolchain directory can be reused by other local
repositories; it does not need to be copied into this repository.

### Recommended MSVC build

Use the one-command wrapper. It installs the portable toolchain if necessary,
injects `cl.exe`, the Windows SDK, libraries, and the correct MSVC linker, then
configures, builds, tests, checks source quality, and starts the smoke program:

```bat
scripts\build-msvc.bat
```


### NAppGUI Frontend on Windows 11

The current desktop Frontend uses NAppGUI. Build the SDK separately with
`NAPPGUI_DEMO=OFF` and `NAPPGUI_WEB=OFF`, then run the reproducible wrapper:

```bat
scripts\build-nappgui-frontend.bat
start build\ui-native\windows-demo\aether_frontend_nappgui.exe
```

Set `NAPPGUI_ROOT` before running the wrapper if the SDK is outside the default
`build\ui-native\nappgui-sdk-msvc-env` directory. This Frontend prototype is
not yet connected to Backend/Core business APIs and does not save credentials
or start Runtime processes.
For a clean CMake configure after changing the compiler or Python environment:

```bat
scripts\build-msvc.bat --clean
```

The equivalent manual sequence is:


```bat
call scripts\msvc-env.bat
cmake --preset msvc
cmake --build --preset msvc
ctest --preset msvc --output-on-failure
uv run --no-project python tools\check_code.py
build\msvc\aether_desktop.exe
```

The environment script uses `PORTABLE_MSVC_ROOT` when set and otherwise the
default path above. It must be called, not merely executed in a child command,
so its `PATH`, `INCLUDE`, and `LIB` changes remain available to CMake.

The MSVC build currently runs nine C tests. The optional `aether_code_quality`
CTest is added only when CMake finds Python; the explicit `uv` command remains
the portable, authoritative quality-check command.

The helper scripts are `scripts/setup-msvc.bat`, `scripts/msvc-env.bat`, and
`scripts/build-msvc.bat`; the reused script license is in
`scripts/msvc-LICENSE.txt`.

### Clang/Ninja preset

The `default` preset requires a `clang` executable available on `PATH`. Use it
only after verifying `clang --version`; otherwise use the MSVC preset above.

```bat
clang --version
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
build\default\aether_desktop.exe
uv run --no-project python tools\check_code.py
```

### Clean rebuild and CMake cache rules

CMake stores absolute compiler and Python paths in the build directory. Do not
reuse a build directory after changing compiler, toolchain, or Python setup.
Delete only the generated build directory and configure again:

```bat
rmdir /s /q build\msvc
call "D:\Applications\msvc-portable\msvc\setup_x64.bat"
cmake --preset msvc
```

If CTest reports that `Python3_EXECUTABLE` points to a missing path, the source
code is not necessarily broken; the build cache is stale. Remove that build
directory, reconfigure, and run the `uv` quality command explicitly.

## Previously verified Windows toolchain

The following versions have been exercised in this repository:

- Windows portable MSVC 19.44.35228, x64.
- CMake 4.4.5.
- Ninja 1.13.2.
- `uv` 0.10.4 running Python 3.13.7 for source checks.

The base C17 targets have also been built and tested on WSL Linux (GCC 13.3).
This does not validate the Linux desktop UI or any macOS build.

## Bounded background execution

`aether_executor` provides copied task inputs, retained results, cooperative
cancellation and generation-tagged monotonic timers. The public contract is
[`executor.h`](include/aether/infrastructure/executor.h). It links Platform and
Foundation only; workers never own Core/Backend/UI state. State owners poll
`aether_executor_take` and `aether_executor_take_timer` and validate target
generations before applying facts. Close admission first, keep callback contexts
alive until close-complete, then join/destroy from a non-UI context.

```text
ctest --test-dir build/msvc -R aether_executor --output-on-failure
```

The executor is not yet wired into Runtime, storage or asynchronous Backend
business commands. A successful worker result is not a business request result.

## Structured message state

Core Message API V2 supports Text, Image/File resource identities, ToolCall,
ToolResult and format-tagged structured data. See
[`message.h`](include/aether/core/message.h). Facts identify Session, Message,
block, Runtime generation and cursor; exact duplicates do not append twice.
Finalizing a block does not complete a message: an explicit normalized END fact
is required. Interrupted/failed streams retain committed partial content.

The former Text-only development API is removed. This is an in-memory domain
contract, not a Runtime protocol adapter, attachment reader or persistence layer.
Windows Frontend must use Backend DTOs rather than calling Core directly.

## Backend queries and permission execution gate

The read-only Backend contract is
[`query.h`](include/aether/backend/query.h): independent request context,
Workspace-scoped access, safe errors with diagnostic IDs, and owned block DTOs.
Only the composition root uses `query_service.h` to bind Core services. Queries
run on the state owner, not a UI callback for large data. Business commands,
full event coverage and durable orchestration remain unfinished tasks.

Permission V2 implements explicit rules, bounded approval waits, session range
memory and a final single-use execution gate. Persistent approval storage and
real Process Service integration are still tracked separately; this is not an
OS sandbox or control over Runtime-internal actions that bypass Aether.

[`async_query.h`](include/aether/backend/async_query.h) adds bounded background
queries with copied request context, receiver generations, cancellation and
close-complete. A single worker exclusively owns the borrowed Query/Core state
while the Client is live; no concurrent direct Core access is allowed. UI code
submits and takes independent results, never waits for a worker or performs a
large synchronous block query. Local state commands and views extend this
Client explicitly below; full business Command/RequestRecord work remains.

Local Workspace/Session views use
[`view.h`](include/aether/backend/view.h): OpenView installs snapshot and
subscription atomically; actual local commands emit revisioned, sequenced events
on a non-UI delivery thread. Full queues invalidate the view explicitly, and
unsubscribe completes only after entered callbacks return. UI receivers must
copy borrowed events and keep their context alive through close-complete.
This local view coverage is not the full business event/Runtime integration.

[`command_request.h`](include/aether/backend/command_request.h) retains local
state-command history independently of work-result polling: Accepted/Running,
one terminal outcome, monotonic deadlines, cancellation intent and separate
actual execution facts. Deadline expiry never erases an already applied state
change. Completion is consumed once; release/expiry cannot reclaim active work.
This local lifecycle does not implement Runtime/permission/persistence workflows.

## Windows managed processes

`aether_process` now builds on Windows with explicit exe/argv/cwd and an exact
environment allowlist, bounded stdout/stderr buffers, copied asynchronous stdin
writes and actual write results. Managed Job trees and borrowed PID observation
have distinct shutdown rights. Stream EOF, root exit and tree cleanup are
separate facts; callers must continue draining during close.

```text
ctest --test-dir build/msvc -R aether_process --output-on-failure
```

The technical API is not a permission grant or OS sandbox. The trusted Backend
execution gate now pins actual executable/cwd/Workspace resources, binds copied
argv/environment to approval, and consumes authorization immediately before
managed spawn. It is not yet wired to Runtime business commands or Frontend.
Non-Windows process backends remain unimplemented. Windows process and gate
sources pass MSVC strict build and code analysis; task records state exact scope.

## Windows HTTP GET transport

`aether_network` uses pinned libcurl 8.23.0 with Windows Schannel,
following the explicit backend decision in ADR-0013. The previous native
WinHTTP experiment failed resource checks even in an independent reproduction;
its code and linkage are removed rather than kept as a fallback.

This is a bounded GET technical target, not the full ENG-07 network service or a
Runtime/model adapter. The contract is in
[`network.h`](include/aether/infrastructure/network.h) and interface contract §29.
Cancellation, total timeout, response limits and cleanup are distinct from HTTP
status. Redirects, environment proxies and ambient authentication are not automatic.
Task records, not this implementation description, establish acceptance results.

The network CTest owns its loopback fixture and ephemeral port; no manual server
or URL argument is required. Failed checks print an error and exit nonzero rather
than opening a CRT assertion dialog. No real credentials or external model calls
are used. See task records for exercised TLS and lifecycle scenarios.

## Repository layout

```text
apps/desktop/                 Desktop composition root
include/aether/foundation/   Foundation public headers
include/aether/core/          Core public headers
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
