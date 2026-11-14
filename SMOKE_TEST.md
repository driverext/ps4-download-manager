# Isolated PS4 startup smoke test

**Developer diagnostic only, not a functional download manager or proven safe on hardware.**

Build #20 reportedly caused a complete PS4 power-off followed by a storage repair at boot. Do not run that release again. GitHub build success does not establish hardware safety.

This test is separate from the main app and uses title ID BREW93571 and Content ID IV0000-BREW93571_00-PS4SMOKETEST0001.

The smoke test uses previously observed working video and controller initialization. It renders simple colored rectangles and exits if Cross is pressed. It makes no calls to the download manager engine, filesystem storage, Real-Debrid, or network transport. It does not write any boot log.

GitHub Actions workflow: PS4 isolated startup smoke test (diagnostic). It produces an artifact for developer inspection and does NOT publish a GitHub Release.

Before considering any hardware test, confirm the console boots normally and other known-good homebrew apps work. If any storage repair, spontaneous shutdown, or instability recurs, stop and address the console stability first. Do not repeat a test after another system shutdown.

The separate main application still requires review of filesystem ABI handling, statvfs, directory reading, I/O, threading, and the application startup order. Passing this smoke test would establish only a narrower graphics/controller launch path, not correctness of the full downloader.
