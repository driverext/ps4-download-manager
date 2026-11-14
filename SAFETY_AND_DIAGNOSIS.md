# Console safety and PS4 filesystem ABI investigation

**DO NOT RUN v0.1.1-ps4-build-20 again.** On one test PS4 it showed a longer-than-usual black screen and the console powered off, requiring the system's storage-repair check at next boot. It may be unsafe on this setup. The earlier build #18 crashed with CE-34878-0 and a boot trace.

## What hardware measured

Build #18 boot trace ended with:

```
FS: before lstat path=/data result=0 errno=0
FS: after lstat path=/data result=-1 errno=78
```

FreeBSD errno 78 is ENOSYS. The OpenOrbis libc's `lstat` uses an unavailable `fstatat` implementation. Build #20 switched to `sceKernelStat`, but passed `OrbisKernelStat` directly. The OpenOrbis v0.5.4 release asset has a documented incompatibility between its stat types and the PS4 kernel's 120-byte layout. Passing the SDK structure directly can cause incorrect field reads or memory-layout problems, and is **not safe to assume correct**.

The updated implementation uses an oversized raw buffer and decodes only kernel-known `st_mode` and `st_size` fields. It also treats `sceKernelStat` failures as kernel errors, not libc `errno`. These changes are **unverified on the user's physical console**. Do not equate GitHub Actions compilation or PKG header validation with launch-time safety.

Relevant upstream analysis: https://github.com/orbis-ports/orbis-compat/blob/master/src/orbis_stat.cpp (the repository is archived and the notes represent the authors' hardware observations, not an OpenOrbis endorsement).

## Safe next steps

1. Keep the PS4 Download Manager uninstalled. Confirm the PS4 boots normally and other known-good apps launch without problems.
2. Preserve `/data/ps4dm_boot.log` if present, and do not delete other console data.
3. Before considering another hardware test, independently review the PS4 SDK ABI, static initialization, and other filesystem calls (`statvfs`, `fopen`, `fsync`, `pthread`) for compatibility.
4. Use a minimal **independently packaged** startup-only diagnostic application, not the full download manager, to verify the toolchain and startup behavior. A clean build by itself is not proof that an app is safe on hardware.
5. Only publish a new release manually after a deliberate review. Automatic release publishing has been disabled.

There is currently **no confirmed safe replacement PKG**.
