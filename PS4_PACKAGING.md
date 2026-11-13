# PS4 PKG repair / CE-32957-6

**Current status:** The repository contains a native PS4 UI in `src/ui/ps4_main.cpp` and PS4 platform code, but the active `CMakeLists.txt` only produces the desktop `ps4dm-cli` binary. There is **no PS4-specific build and packaging workflow** in the current repository. A desktop binary or renamed archive cannot be installed as PS4 homebrew.

## Build requirements

Use the [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) with libraries/headers for the native target. Build PS4-specific code: `src/ui/ps4_main.cpp`, `src/platform/graphics.cpp`, `src/platform/ps4_http.cpp`, `src/platform/ps4_abi.c` and core modules. Do not assume the host `CMakeLists.txt` generates a PS4 executable. Compile and link a valid PS4 ELF, create a PS4 `eboot.bin`, and stage this layout:

```
staging/
  eboot.bin
  sce_sys/
    param.sfo
    icon0.png
```

Then use an OpenOrbis-compatible GP4/PKG workflow. The GP4 project Content ID and `sce_sys/param.sfo` Content ID must match. Example **illustrative** 36-character Content ID:

```
IV0000-BREW00001_00-PS4DOWNLOADMGR01
```

Title ID in metadata must be consistent with `BREW00001` and must not conflict with an installed title. A correctly formatted ID does not alone guarantee that the package is valid or compatible.

## Inspect existing PKG before FTP

Run in PowerShell from the repo:

```powershell
py -3 scripts/check_pkg.py "C:\path\to\PS4DownloadManager.pkg" --sha256
```

Use `--expected-id IV0000-BREW00001_00-PS4DOWNLOADMGR01` **only if that is the ID configured in your GP4 and SFO**. Otherwise the checker will legitimately flag a mismatch.

The checker verifies magic bytes, the Content ID header field, plausible entry-count/table offset, and the declared package size. It is a **limited header preflight**, not a complete verifier. To establish installability, separately inspect the `param.sfo`, executable, GP4 and use a trusted PS4 PKG validation tool.

If the checker fails, regenerate the package instead of FTP-uploading it again. If it passes but installation fails, compare local/remote SHA-256 or size to rule out interrupted FTP transfer. Try a known-good homebrew package through the same installation workflow to isolate the PS4/FTP setup.

**No fixed installable .pkg is included in this change.** A PS4 SDK/toolchain build and examination of the rejected PKG are still needed.
