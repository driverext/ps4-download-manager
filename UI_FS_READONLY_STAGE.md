# Filesystem integration stage: semi-complete native UI

This build retains the PS4 Download Manager interface that already worked on the user's PS4, and adds **one explicitly triggered, read-only** filesystem check.

- Separate title ID: BREW93574.
- Content ID: IV0000-BREW93574_00-PS4FSREADONLY001.
- In the application, press Options to open Settings. Select "Probe /data (READ-ONLY; press X)" and press X once.
- The bottom status message becomes "FS PROBE OK: /data is a directory" on success or a hex error code otherwise.
- No filesystem calls are made automatically on startup, and the test never creates or modifies a directory or file. Network, engine, Real-Debrid, actual downloads, and persistent settings remain disabled.
- The syscall is sceKernelStat, using an aligned 256-byte buffer instead of the SDK's possibly incompatible stat structure. Its directory mode is read at byte offset 8.
- In-app teardown is not validated. Use PS4 home/app switching controls, and avoid repeating tests if it crashes.
- This is unverified on hardware. An earlier full app caused a complete console shutdown and storage repair. **Do not use the filesystem probe until your console is stable with other known-good software. Stop testing after any new abnormal shutdown.**

The next stage must wait for the hardware result. Do not re-enable directory creation, metadata writes, or background download processing in this same test.
