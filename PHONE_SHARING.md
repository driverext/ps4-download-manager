# Phone-to-PS4 link sharing (experimental, beta 0.3.0)

The PS4 web interface is a private-LAN page served by the running download manager itself at:

`http://192.168.1.150:8787`

This IP was supplied by the PS4 owner. **If the console's IP changes, the page will not work until updated in source.** Both devices must be on the same Wi-Fi/LAN, with no guest-Wi-Fi client isolation. Do not expose TCP port 8787 to the public internet or forward it through a router.

## Setup

1. Install the distinct beta 0.3.0 PKG (title ID `BREW93577`) and open the native app.
2. Press Options and choose **HTTPS networking: ENABLE**.
3. Choose **Phone sharing: ENABLE**. The app should display `PHONE READY: http://192.168.1.150:8787`.
4. From iPhone Safari, visit `http://192.168.1.150:8787`, paste a direct HTTPS link and tap **Send for approval**.
5. On the PS4, use Options to return to Settings. Select **Storage and queue: ENABLE** if not already enabled.
6. Select **Approve phone link**. Only after this controller approval does the app resolve and queue the link. Queue displays the job. Select **Reject phone link** to discard it instead.

## Safety

The embedded HTTP server is bounded: only GET / and POST /submit are implemented; only a 4 KiB form body is accepted. There is one pending link; submissions cannot start transfers automatically and a full pending slot returns HTTP 409. URL parsing rejects invalid schemes and credentials. The approval path additionally requires HTTPS. This is local HTTP, not HTTPS, and the text link is visible to others on the LAN. Do not paste sensitive authenticated URLs or publish the bridge on the internet.

The network listener is **disabled by default**, requires explicit selection, and does not initialize at app startup. Console crashes during shutdown remain unresolved. Native PS4 network socket, HTTP transport, queue metadata and downloads remain hardware unverified; past full builds caused a hard shutdown and storage repair. Stop immediately if instability returns.
