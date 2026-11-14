# Beta 0.3.2: download worker crash mitigation

## Observed hardware evidence
- v0.3.0: iPhone web page works, pressing PS4 Approve phone link causes CE-34878-0.
- v0.3.1: same link is added to the persistent queue successfully as **paused**. Pressing Square to resume still causes CE-34878-0.
- Earlier full-app build caused a complete console power-off, then a storage repair prompt. Avoid repeated crash testing.

## Change
- The download engine no longer constructs a fresh PS4 network pool, SSL context and HTTP context inside each worker. It receives a pointer to the PS4 HTTP transport already initialized by **HTTPS networking: ENABLE**, and uses that shared transport.
- The HTTP transport serializes requests under a mutex so Real-Debrid and download operations do not simultaneously access the same context.
- Worker status is updated in memory before key phases: destination check, partial metadata read, HTTPS request, metadata write, destination open, sync, and completion metadata. Queue rows display the most recent phase if the UI survives.
- No automatic job resume on launch; phone approval still enqueues a paused download. Maximum download size remains 64 MiB.
- Distinct BREW93579 PKG.

## Important
This changes a plausible root cause but **does not prove** the PS4 thread or HTTP APIs are now safe. A crash can occur before the new phase can be observed. A 64 KiB worker stack, unsafe libc file operations, and PS4 HTTP ABI differences remain possibilities.

Only consider one small-file test on an otherwise stable console with no new storage repair warning. Do not retry after a crash or power-off. Compilation and GitHub Actions passing are not hardware tests.
