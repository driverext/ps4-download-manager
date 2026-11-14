# Phone approval crash fix in beta v0.3.1

Observed hardware result for beta v0.3.0: the LAN page at `http://192.168.1.150:8787` worked from iPhone, but pressing **Approve phone link** caused console application crash `CE-34878-0`.

## Root-cause hypothesis

The v0.3.0 approval code immediately invoked `DirectResolver::resolve(link)`, which synchronously issues an HTTPS `HEAD` request using PS4 Http/SSL calls **inside the game's input/render thread**, before `Engine::add`. A crash at this step implicates the HEAD request or adjacent code, but the exact faulting call cannot be confirmed without a stack trace or on-device logs.

## v0.3.1 change

Approval no longer sends a network request. It takes the validated HTTPS URL directly from the pending phone request and calls `Engine::add(url, "", "From iPhone", true)`, where the final boolean creates a **paused** queue record. The next UI frame will not start a download worker. The queue entry is persisted using the existing storage engine, which also remains unverified on hardware. After confirming queue presence, a separate **Square** button resumes the selected job and starts the existing download worker.

For user's earlier test URL `https://raw.githubusercontent.com/github/gitignore/main/README.md`, expect a paused entry named `README.md`, not an automatic download.

### Operating procedure

1. Launch v0.3.1 and enable Storage and queue, HTTPS networking, then Phone sharing.
2. Open the page from Safari and send a small public HTTPS URL.
3. Press Approve phone link on PS4. The UI should show **PHONE LINK QUEUED PAUSED** and Queue should display the paused job. No HEAD or GET is made at approval.
4. **Do not resume downloads if the console has become unstable.** The native PS4 HTTPS transport and disk writes remain hardware unverified. If stable and willing to test, select the paused job and press Square to resume. Stop after any abnormal shutdown.

The package uses its own title ID `BREW93578` and does not replace prior diagnostics. This change addresses a risky code path, not a confirmed full fix or hardware safety guarantee. Do not repeat v0.3.0 approval tests.
