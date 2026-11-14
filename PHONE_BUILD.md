# Build a PS4 PKG from your phone

No Codex, Windows computer, or local compiler required. GitHub Actions runs the OpenOrbis toolchain in the cloud.

## On iPhone

1. Visit https://github.com/driverext/ps4-download-manager/actions
2. Choose **Build PS4 PKG (phone friendly)**.
3. Tap **Run workflow**, choose main, then confirm.
4. Open the latest run. Only proceed if it shows a **green check**.
5. Scroll down to **Artifacts**, download **PS4DownloadManager-GoldHEN** (ZIP).
6. In iPhone Files, tap the ZIP to unzip it. Extract the file ending in **.pkg**, not the ZIP.
7. Connect your phone FTP client to the GoldHEN FTP server and transfer the .pkg to **/data/pkg/** using binary transfer mode. Verify that the FTP file size matches.
8. On PS4 use GoldHEN's local Package Installer to install from /data/pkg/.

**If the workflow fails or has no artifact:** Open the failed step and copy its logs. Do not try installing an absent or partially produced PKG. A green workflow does not guarantee PS4 compatibility until tested on the console.

## What's being built

The original repository's CMake target builds the desktop command-line application. **Makefile.ps4** instead builds the native PS4 UI, OpenOrbis platform networking/graphics, download engine and Real-Debrid module, then generates an eboot and package.

The package uses Title ID \`BREW00001\` and Content ID \`IV0000-BREW00001_00-PS4DOWNLOADMGR00\` consistently. This is a *new identity* relative to the old experimental package.

This project is experimental. OpenOrbis headers, libraries, or third-party dependencies may cause compiler or runtime failures; examine the logs and fix these rather than treating an artifact as proven installable.
