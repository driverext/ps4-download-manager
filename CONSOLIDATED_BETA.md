# PS4 Download Manager 0.2.0: consolidated beta

This is a **single experimental PKG** built from the actual native PS4 download manager, not a rendering smoke test. It consolidates persistent queue storage, network transport, bounded direct HTTP(S) downloads and optional Real-Debrid into one app.

Because the user's earlier full build caused a console power-off followed by storage repair, these components are **not initialized automatically on launch**. The confirmed-working UI remains the startup path. Build success cannot prove console safety.

## On-console sequence

1. Open the beta. It should display the established native UI.
2. Press **Options** to open Settings. Scroll to the last three entries.
3. Select **Storage and queue: ENABLE**. This creates the app directory and downloads directory and loads/creates the queue file via the existing storage engine.
4. Select **HTTPS networking: ENABLE**. This initializes native PS4 network/SSL/HTTP services.
5. Go to **Add download**, select **Enter URL or magnet**, type a public direct HTTPS file URL and finish with the on-screen keyboard's **DONE** button. It should queue the download and show progress in Queue.
6. Optional later: Settings > **Real-Debrid: ENABLE** and then the Real-Debrid account menu to authorize a device.

If a step fails, report the exact step and the error message. **Do not repeat a step after a full console shutdown or new storage repair warning.**

## Safety limitations

- Unique title ID BREW93576, separate from the earlier packages.
- Transfer size limited to **64 MiB per file**, including unknown-length transfers.
- One concurrent transfer, no automatic retries, automatic resume disabled in the beta startup configuration.
- Download destination fixed to `/data/PS4DownloadManager/downloads` when the storage engine is first enabled.
- Filesystem free-space preflight is disabled in this beta because the existing `statvfs` interface has not been verified with the hardware ABI. The kernel can still fail writes when the storage device is full. Ensure ample free space and only test small, expendable files.
- No automatic filesystem or networking initializers on app startup.
- The full app's graceful teardown path is not fixed; use normal PS4 Home controls, and stop if error CE-34878-0 occurs.
- The existing HTTP, thread, queue persistence, JSON I/O and Real-Debrid implementations remain **hardware unverified**. Real-Debrid must not be assumed functional merely because the PKG compiles.
- Do not use the beta for important files or store secrets until reliability has been confirmed.

This beta is intended to reduce the number of installations while still allowing different subsystems to be activated separately within the same application.
