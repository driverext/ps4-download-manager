# PS4 Download Manager: staged hardware integration

Stage 1 (startup / rendering / controller): separate title ID BREW93571. The original variant successfully rendered bars but crashed with CE-34878-0 on X because X exited the main loop. The revised v0.1.1 makes X change a bar color instead of exiting. Test of the revised version on console is not yet confirmed.

Stage 2 (read-only filesystem ABI): separate title ID BREW93572. Renders the same bars. A yellow fourth bar means the filesystem probe has not run. Press **SQUARE** once to issue a single read-only sceKernelStat("/data") using 256-byte aligned kernel output storage, then interpret only the mode field at offset 0x08. The fourth bar becomes green if this succeeds and reports a directory, red otherwise. X changes the third bar color and does not exit. No storage files are created or modified. This does **not** use the full download manager storage library, create directories, or access network services.

**Important safety:** An earlier full build caused a complete power-off and PS4 storage repair. Compilation and package checks cannot prove a diagnostic is safe on hardware. Only consider testing after the console has shown stability with other known-good software. Do not continue testing after a fresh shutdown or storage-repair event. The file-stat probe is optional: simply launching the app should render bars without invoking it. The app has no built-in exit because the cleanup path previously crashed.

Future stages, contingent on hardware results:
- Stage 3: isolated networking initialization with no internet request and no persistent writes.
- Stage 4: HTTP request and bounded data download to an isolated directory.
- Stage 5: background download queue and retry/resume, with tested storage primitives.
- Stage 6: Real-Debrid device authorization and link resolution.

Each stage should get its own title ID and test artifact. Do not ship an integrated release until each prerequisite succeeds on hardware and the exit/cleanup crash has a verified fix.
