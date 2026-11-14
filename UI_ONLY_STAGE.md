# Semi-complete native PS4 Download Manager: offline UI integration stage

This stage compiles the **actual existing download-manager UI** from `src/ui/ps4_main.cpp`, including the Home page, Add download form, queue, downloads screens, Real-Debrid menu, torrents menu, files menu, settings, and on-screen keyboard. It uses the original controller and video code.

What is deliberately disabled using `DM_UI_ONLY=1`:
- No persistent storage directories, read/write, filesystem checks or boot log.
- No network transport or HTTP/SSL library initialization.
- No engine, download queue processing, downloads, or Real-Debrid login.
- Settings are adjusted in memory only. File listing shows an offline placeholder.
- Selecting an actual download will show an initialization error, by design.
- There is no safe, confirmed in-app teardown path. Exit through normal PS4 controls; avoid repeated tests if it crashes.

Build identity: `BREW93573`, package `IV0000-BREW93573_00-PS4UIONLYSTAGE01.pkg`. It installs independently and does not replace the prior full app or smoke test.

The original full download-manager build caused a shutdown followed by storage repair. This package is not proven safe on physical hardware, though it intentionally bypasses the startup path associated with the earlier crash. Compilation and PKG validation in GitHub Actions are not hardware tests.

### Isolation order
1. Native graphics/controller smoke test (screen confirmed, original X shutdown crashed).
2. This semi-complete menu UI with **zero** storage/network initialization.
3. Read-only filesystem `sceKernelStat` probe (distinct PKG; optional SQUARE action).
4. Bounded storage write/queue persistence in a dedicated app-owned directory.
5. Network initialization and HEAD request.
6. Direct download transfers, pause/resume, then Real-Debrid.

Use this step to determine whether the native full UI and navigation are stable before reintroducing any disk or network operations.
