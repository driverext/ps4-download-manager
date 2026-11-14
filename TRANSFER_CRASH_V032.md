# PS4 download transfer crash investigation, v0.3.2

Observed on hardware:
- PS4 native UI works.
- Read-only stat and isolated directory creation work.
- iPhone Safari can connect to PS4 link server.
- Adding a URL through PS4 Approve succeeds in v0.3.1 and leaves it PAUSED.
- Pressing Square to resume immediately crashes with CE-34878-0.

## Source-level defect found

`src/platform/ps4_http.cpp` allocated `char buffer[65536]` inside `SceTransport::perform`. This runs inside `Engine::run` on an `std::thread`. PS4 default worker stacks may be only 64 KiB, so this large local buffer plus other frames and TLS could overflow before the first download response is read.

The fix replaces the stack array with `std::vector<char> buffer(16*1024)`, allocated on the heap. Download callback uses `buffer.data()` and `buffer.size()`.

This is a concrete source-code compatibility issue, **not definitive proof of the PS4 crash root cause**. Threads, HTTP context initialization, TLS and filesystem writes are still unverified. If a new crash occurs after this fix, do not repeat it; collect a backtrace or use an in-memory stage indicator.

Retains existing safety: manual storage/network/phone initialization, phone link enters queue paused, 64 MiB maximum file, no background startup until Square. Prior full builds caused console power-off and storage repair; do not test further on unstable hardware.
