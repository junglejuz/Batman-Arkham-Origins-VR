# Batman Origins VR

Read `../VR_ESSENTIALS.md` and `../SESSION_START.md`, then `ORIGINS_PLAN.md`.
Current state: initial VR implementation compiled; read `README.md`,
`BUILD_PLAN.md` and `RE_NOTES.md`. Native/AFW/DLSS code is in the build;
deployment receipt is `re/evidence/installed-build.json`. O1B's first VDXR run
produced thousands of native pairs, no tag misses/aborts in the sampled log;
JJ reported HUD fusion and DLSS edge-quality failures. O1H is installed; it
replays JJ's private City bat recording on Origins' guarded exact Wwise events.
First title exit starts fully black and reveals; later events cover, hold over
loading and reveal. Coverage is head-relative and full-FOV. Original draws are
preserved; near-black coverage is filtered from the UI only after successful
XR copy, leaving coloured menu content beneath the bats. O1F's blanket UI skip
is removed. Title prompts remain visible before the first event. The first two
events auto-capture raw UI, preserved UI and coverage once at >=400 ms; periodic
status distinguishes generated, copied and xrEndFrame-accepted bat frames.
O1H is now installed: O1G bats returned but remained small (JJ); its live log
shows generated frames but zero copied/submitted, so the original panel bats
were the only animation delivered. O1H's shared runtime-image transfer handles
TYPELESS RGBA copies and explicit typed conversion views. Actual GPU transfer
checks reproduce the old null-view failure and pass typed sRGB/TYPELESS RGBA/BGRA
shapes/alpha/black endpoints. Requested/returned image formats and failure stages
are logged. O1H live/headset validation is pending; the old run did not record
the actual resource descriptor. See O1H results and receipts.
JJ rejected O1D's four small windows and O1E's skipped gameplay bats / square
first reveal; O1F still had missing bats. O1G's full-field layer was never copied.
Private animation must stay local.
O1C-last-live.log records native pairs, the triangle HUD boundary and actual
per-eye NGX output; HUD/AA visual acceptance and O1D transition contexts remain
pending. Never call build/offline checks live proof. See O1H results in BUILD_PLAN.md.

Match the current Arkham City conventions. Native same-frame engine stereo is
the default; AFW is an explicitly selectable alternative. DLSS Super Resolution
and both VDXR and SteamVR are required planned capabilities.

City/Knight addresses, ABI layouts and shader identities do not transfer.
Fingerprint Origins and prove each seam before modifying it. Keep SOURCE,
STATIC, LIVE/sim and HEADSET evidence distinct. Record external-file edits in
`FIX_CHANGES.md` and prepare grouped live-switch test plans before any launch.

Playbook: `../references/vr-modding-playbook/AGENTS.md`; use routes in
`../references/README.md` and cite the checkout commit, section and pattern or
failure ID in this project's own notes. Do not put Origins notes upstream.
