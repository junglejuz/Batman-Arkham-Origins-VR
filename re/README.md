# Origins read-only investigation

Target fingerprint and evidence boundaries: [RE_NOTES.md](../RE_NOTES.md).
These tools do not build, inject or launch a mod.

From the workspace root, with the existing Python `pefile`/`capstone` packages:

```powershell
python batman_origins/re/origins.py --out batman_origins/re/evidence/static-command-map.json
python batman_origins/re/origins.py --views --out batman_origins/re/evidence/static-view-candidates.json
python batman_origins/re/origins.py --dis 0x00665560 --count 20
python batman_origins/re/origins.py --refs 0x01492ec8
```

The scanner rejects builds with a different SHA-256. Addresses are
preferred-image virtual addresses; runtime hooks must use actual module base
plus RVA and validate loaded bytes. Raw byte matches and linear disassembly
are leads; they do not prove live ownership, complete ABI or stereo safety.

With Origins' own GhidraMCP bridge listening on port 8082:

```powershell
python batman_origins/re/ghidra_read.py --port 8082 --out batman_origins/re/evidence/ghidra-preflight.json
python batman_origins/re/ghidra_read.py --action function --address 0067ed90
python batman_origins/re/ghidra_read.py --action decompile --address 0067ed90 --out batman_origins/re/evidence/ghidra-viewport-root.json
python batman_origins/re/ghidra_read.py --action xrefs --address 01492ec8
```

The direct HTTP reader uses the same GET endpoints as the supplied MCP bridge.
It checks Origins section starts and a command-descriptor anchor, then saves raw
receipts. It performs no Ghidra writes and does not switch another instance.
The identity guard is not a complete hash of the Ghidra-imported program.

The supplied MCP subsequently switched to Origins on port 8080. Use the port
whose preflight actually passes; availability and selected program can change.
While auto-analysis runs, the guard may reject missing function definitions.
Wait for it to finish before considering additional seeds.

If analysis finishes without defining these functions, add this `re` directory
to Ghidra's Script Manager script directories, then run
`OriginsDefineRenderCandidates.java` once. It validates the program hash and
all ten 16-byte prefixes, preserves existing functions, and defines only missing
candidate functions. It sets no prototypes and edits only Ghidra's database.
Both Java scripts have now compiled and run successfully in the separate
`headless/OriginsRender_20261011` project; 10/10 functions completed decompilation.
They have not run in the user's open project. Read the console result before
trusting any names; `CANDIDATE` labels remain unproven semantics.

Focused output is in [ghidra-focused-render.txt](evidence/ghidra-focused-render.txt).
`OriginsDumpRenderCandidates.java` expects one argument: the output text path.
Decompiled types are provisional; check stack cleanup and call-site assembly
before adopting a hook signature. The GUI auto-analysis failure and separate
headless workaround are recorded in the project notes.

If several CodeBrowsers run, each bridge needs a distinct port. The supplied
MCP transport's default port 8080 initially belonged to Unity; Origins was open
in another process. Set Origins' `GhidraMCP HTTP Server → Server Port` in Tool
Options and reload the plugin before using its port. Re-run preflight whenever
the active program changes.
