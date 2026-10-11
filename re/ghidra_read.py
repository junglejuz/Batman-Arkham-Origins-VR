"""Read-only access to a selected GhidraMCP HTTP bridge, with an Origins guard.

The MCP transport here uses port 8080 (initially Unity, subsequently Origins).
A second Ghidra instance needs its own listener. This client uses the identical bridge GET endpoints,
without altering the MCP configuration or any program, label or prototype.
"""
import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path
from urllib.error import URLError
from urllib.parse import urlencode
from urllib.request import urlopen

from origins import SHA256

EXPECTED_SECTIONS = {".text": 0x401000, ".rdata": 0x1395000,
                     ".data": 0x17C0000, ".bind": 0x32EB000}


def get(port, endpoint, params=None):
    query = "?" + urlencode(params) if params else ""
    # Localhost only; selecting a port never changes a running bridge.
    with urlopen(f"http://127.0.0.1:{port}/{endpoint}{query}", timeout=30) as response:
        return response.read().decode("utf-8")


def guard(port):
    segments = get(port, "segments", {"limit": 40})
    observed = {}
    for line in segments.splitlines():
        match = re.match(r"([^:]+):\s*(?:0x)?([0-9a-fA-F]+)\s*-", line)
        if match:
            observed[match[1].strip()] = int(match[2], 16)
    for section, expected in EXPECTED_SECTIONS.items():
        if observed.get(section) != expected:
            raise ValueError(f"Wrong Ghidra program at port {port}: {section} starts at "
                             f"{observed.get(section)!r}; Origins expects {expected:#x}")
    descriptor = get(port, "disassemble_function", {"address": "0x00665570"})
    if not re.search(r"(?:0x)?0*14915b4\b", descriptor, re.IGNORECASE):
        raise ValueError("Origins BeginDrawingCommand descriptor anchor did not match")
    return {"segments": segments, "descriptor": descriptor}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8082)
    parser.add_argument("--action", choices=("preflight", "decompile", "disassemble", "function", "xrefs", "strings"), default="preflight")
    parser.add_argument("--address", help="Preferred-image hex VA")
    parser.add_argument("--filter", help="String substring")
    parser.add_argument("--limit", type=int, default=100)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("invalid port")
    try:
        preflight = guard(args.port)
        params = {}
        if args.action == "preflight":
            result = preflight
        else:
            endpoints = {"decompile": "decompile_function", "disassemble": "disassemble_function",
                         "function": "get_function_by_address", "xrefs": "xrefs_to", "strings": "strings"}
            if args.action == "strings":
                params = {"offset": args.offset, "limit": args.limit}
                if args.filter:
                    params["filter"] = args.filter
            else:
                if not args.address:
                    parser.error("--address required")
                params["address"] = f"0x{int(args.address, 16):08x}"
                if args.action == "xrefs":
                    params.update(offset=args.offset, limit=args.limit)
            result = get(args.port, endpoints[args.action], params)
    except (URLError, ValueError, TimeoutError) as error:
        parser.exit(1, f"Ghidra preflight/read failed: {error}\n")
    receipt = {"grade": "STATIC/Ghidra", "utc": datetime.now(timezone.utc).isoformat(),
               "bridge": f"127.0.0.1:{args.port}", "action": args.action, "parameters": params,
               "installed_executable_sha256_reference": SHA256,
               "identity_scope": "Section starts plus descriptor anchor, not a full Ghidra-memory hash",
               "preflight": preflight, "result": result}
    text = json.dumps(receipt, indent=2, ensure_ascii=False) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"Saved {args.out}")
    else:
        print(text)


if __name__ == "__main__":
    main()
