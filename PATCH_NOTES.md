# TrafficMonitor — stabilization and modernization patch

This package is a modified version of the uploaded `TrafficMonitor-master.zip`.
It is a *source patch*, not a prebuilt Windows executable. MFC/MSBuild are
not available in the environment in which the patch was prepared.

## Build (Windows)

From the project root, double-click `build.bat`, or run:

```bat
build.bat
build.bat x64 full Release
build.bat x86 lite Debug
build.bat ARM64EC lite Release
build.bat x64 lite Release -NoUpdate
build.bat x64 lite Release -NoUpdate -WarningsAsErrors
```

Defaults: **x64**, **lite**, **Release** (uses `TrafficMonitor_Lite.sln`).

The bootstrapper (`tools\build.ps1`) requires Windows PowerShell 5.1 or later,
an internet connection for missing/updateable dependencies and appropriate
privileges/UAC approval. It:
1. Detects installed VS 2022 (`v143`) through `vswhere`.
2. Installs VS 2022 Build Tools if missing, via `winget`; falls back to the
   official Microsoft installer if `winget` is missing.
3. Checks for updates to the installed VS 2022 edition with `winget` unless
   `-NoUpdate` is specified. Updates of VS installations unmanaged by
   `winget` may need to be performed via Visual Studio Installer.
4. Adds missing MSVC, MFC and Windows 11 SDK components.
5. For **full** builds, also requests C++/CLI and .NET Framework 4.7.2
   targeting pack/SDK for `OpenHardwareMonitorApi`.
6. Runs the original solution through MSBuild and reports success/failure.

**Rationale for VS 2022 instead of automatically switching to VS 2026:**
The provided `.vcxproj` files explicitly target platform toolset `v143`.
Installing an unrelated newer toolset can break the build. The script updates
the latest available *compatible VS 2022* installation, not the compiler
major version; switching to another MSVC toolset requires a separate port.

Third-party `OpenHardwareMonitorApi/LibreHardwareMonitorLib.dll` is included
in the uploaded source and is deliberately **not** replaced with an
unreviewed "latest" binary: that could introduce ABI/API and supply-chain
regressions. Its version remains pinned to the input archive.

## Build log: compile blockers and MSVC warning fixes

The follow-up package also fixes source-level MSVC diagnostics from the supplied
Windows build log. See `WARNING_FIXES.md` for the fatal errors, warning categories
and affected files. `build.bat` can now write a `.binlog` diagnostic file and
optionally treat all warnings as errors via `-WarningsAsErrors`.

**The native Windows build has not been re-run here**; reported warning locations
were patched, but compiler verification is still required on Windows.

## Implemented source changes

| File | Change |
|------|--------|
| `TrafficMonitor/NetworkSampling.h` | New RAII-safe bounded `GetIfTable()` retry and `GetIfTable2()` interface-index keyed 64-bit counters. Validates API results; deduplicates selected interfaces; refuses incomplete aggregates. |
| `TrafficMonitor/TrafficRateMath.h` | Pure C++20 saturating bytes-per-second math, handling counter resets and nonpositive intervals. |
| `TrafficMonitor/TrafficMonitorDlg.cpp:399` | Initialize only from a successful complete table, no fallthrough with invalid buffer. |
| `TrafficMonitor/TrafficMonitorDlg.cpp:498` | Resolve interface identity (`dwIndex`) if the row order changes; avoid stale row indexes. |
| `TrafficMonitor/TrafficMonitorDlg.cpp:1214` | 64-bit network counters, monotonic elapsed time, separate direction reset guards, clear stale speeds on failed reads, prevent zero-interval division. |
| `TrafficMonitor/TrafficMonitorDlg.cpp:1565` | Event-driven worker instead of polling every 10ms; atomics for exit/pending flags; orderly shutdown with message pumping. |
| `TrafficMonitor/TrafficMonitorDlg.cpp:2119` | Independent interface-table snapshot for modal network details to prevent reallocating the dialog's backing buffer. |
| `TrafficMonitor/AdapterCommon.cpp:18` | RAII for legacy adapter-enumeration buffer, no raw `new[]`/`delete[]`. |
| `TrafficMonitor/AdapterCommon.cpp:52` | Linear-time IP refresh keyed by descriptions while preserving existing user settings. |
| `TrafficMonitor/AdapterCommon.cpp:73` | Validate `-1`/out-of-bounds row matches; record stable interface IDs for subsequent sampling. |
| `TrafficMonitor/AdapterCommon.h` | Initialize row index to `-1`, initialize counters, record interface index. |
| `TrafficMonitor/NetworkInfoDlg.cpp:139` | Guard against negative row access when no interface matches. |
| `TrafficMonitor/TaskBarDlgDrawCommon.h:782` | Stoppable/joined cache GC using C++20 `std::jthread`, stop-aware condition variable, and no operator[] insertion after failed resource creation. |

## Tests executed while preparing the patch

- `g++ -std=c++20 -Wall -Wextra -Werror tests/TrafficRateMathTest.cpp`
- Portable traffic-rate tests: **PASS**
- XML parse of bundled `.vcxproj` configurations: **PASS**
- Full MSBuild compile: **NOT RUN** (Windows, MSVC, MFC and Windows SDK
  are unavailable in this environment).
- Windows runtime tests: **NOT RUN**.

Test manually on Windows: adapter hotplug, Wi-Fi changes, VPN transitions,
multi-adapter selection, counter resets, >4 GB received within a sampling
period, sleep/resume, render-target recreation, close while sampling, and
full/lite x64 and x86 builds.

## Remaining risks / follow-up (not claimed to be fixed)

- **Thread affinity/data races:** Some pre-existing monitoring code still
  updates the connection menus/configuration via `IniConnection()` from the
  worker while the main window can read or modify adapter state. A full
  thread-safe snapshot ownership/UI-message refactor is recommended before
  claiming thread safety. Avoid holding locks around synchronous UI messages.
- **Legacy IPv4 enumeration:** `GetAdaptersInfo()` was made memory-safe but
  deliberately retained to keep existing description matching and persisted
  adapter preferences stable. An end-to-end `GetAdaptersAddresses()` migration
  would improve IPv6 support after settings migration is designed.
- **AutoSelect:** The initial active-interface heuristic still consults
  legacy 32-bit initial counters; the *speed* path uses 64-bit counters.
- **UI-render profiling:** Cache lifecycle defects were addressed but actual
  CPU/GPU optimization needs Windows ETW/WPA or Visual Studio profiling.
- **Full build and native ARM64EC:** Additional C++/CLI and mixed-platform
  configuration issues are possible and require testing on their target.
- **Security:** This is not a complete security audit of plugins, update
  URLs, arbitrary DLL loading, or bundled third-party dependencies.

This report distinguishes code fixes from unverified runtime behavior:
passing portable tests does not establish a successful native Windows build.
