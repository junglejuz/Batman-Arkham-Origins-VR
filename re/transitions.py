"""Read-only Origins transition evidence; game files are never written.

UE3 chunk-layout lead: UEViewer Unreal/UnrealPackage/UnPackage3.cpp
https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealPackage/UnPackage3.cpp
Package evidence is an analysis buffer, not a loadable replacement package.
Requires existing pefile/capstone and analysis-only lzokay 2.1.0.
"""
import hashlib
import json
import math
import re
import struct
import zlib
from pathlib import Path

import lzokay
from origins import Target, EXE

ROOT = Path(__file__).resolve().parent
GAME = EXE.parents[2] / "BMGame" / "CookedPCConsole"
EVENT = "Play_UI_Fader_Bats"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def event_id(name):
    # Confirmed in Origins GetIDFromString: lowercase ASCII, FNV-1 (not FNV-1a).
    value = 0x811C9DC5
    for c in name.lower().encode("ascii"):
        value = ((value * 0x1000193) & 0xFFFFFFFF) ^ c
    return value


def package(path):
    source = path.read_bytes()
    magic = struct.pack("<I", 0x9E2A83C1)
    chunks = []
    for match in re.finditer(re.escape(magic), source):
        at = match.start()
        if at + 16 > len(source):
            continue
        _, block, compressed, size = struct.unpack_from("<IIII", source, at)
        if block != 131072 or not size:
            continue
        count = math.ceil(size / block)
        end = at + 16 + count * 8 + compressed
        if end > len(source):
            continue
        # The summary's compressed-chunk record owns this physical container.
        record = source.find(struct.pack("<II", at, end - at), 0, 0x1400)
        if record < 8:
            continue
        offset, expected = struct.unpack_from("<II", source, record - 8)
        if expected != size:
            continue
        pos = at + 16 + count * 8
        decoded = bytearray()
        for i in range(count):
            cs, us = struct.unpack_from("<II", source, at + 16 + i * 8)
            assert 0 < us <= block and pos + cs <= end
            decoded.extend(lzokay.decompress(source[pos:pos + cs], us))
            pos += cs
        assert pos == end and len(decoded) == size
        chunks.append((offset, decoded, record - 8))
    assert chunks, f"No validated LZO chunks: {path}"
    table = min(record for _, _, record in chunks)
    count = struct.unpack_from("<I", source, table - 4)[0]
    assert count == len(chunks) and {record for _, _, record in chunks} == {table + i * 16 for i in range(count)}, "Chunk candidates must exactly equal the package summary array"
    data = bytearray(source)
    for offset, decoded, _ in chunks:
        if offset + len(decoded) > len(data):
            data.extend(bytes(offset + len(decoded) - len(data)))
        data[offset:offset + len(decoded)] = decoded
    data = bytes(data)
    output = ROOT / "evidence" / ("O1D-" + path.stem + "-unpacked.upk")
    if output.exists():
        previous = output.read_bytes()
        assert previous == data, f"Reconstruction differs: {output}"
    else:
        output.write_bytes(data)
    movies = []
    for match in re.finditer(b"CFX", data):
        at = match.start()
        try:
            expected = struct.unpack_from("<I", data, at + 4)[0]
            if expected < 8 or expected > 64 * 1024 * 1024:
                continue
            swf = zlib.decompress(data[at + 8:])
            if len(swf) + 8 != expected or EVENT.encode() not in swf:
                continue
            markers = [s for s in (EVENT, "Fader", "FE_PressStart", "FE_UnPause", "Intro", "Outro")
                       if s.encode() in swf]
            movies.append({"virtual_offset": hex(at), "uncompressed_swf_bytes": expected,
                           "markers": markers, "swf_body_sha256": digest(swf)})
        except (zlib.error, struct.error):
            continue
    return {"source": str(path), "source_sha256": digest(source), "chunks": len(chunks),
            "analysis_buffer": str(output), "analysis_sha256": digest(data), "movies_with_event": movies}


def bank(path, wanted):
    data = path.read_bytes()
    pos = 0
    matches = []
    while pos + 8 <= len(data):
        tag = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        end = pos + 8 + size
        assert end <= len(data)
        if tag == b"HIRC":
            n = struct.unpack_from("<I", data, pos + 8)[0]
            obj = pos + 12
            for _ in range(n):
                kind, length = struct.unpack_from("<BI", data, obj)
                assert length >= 4 and obj + 5 + length <= end
                ident = struct.unpack_from("<I", data, obj + 5)[0]
                if ident == wanted:
                    matches.append({"object_type": kind, "id": hex(ident), "offset": hex(obj)})
                obj += 5 + length
            assert obj == end
        pos = end
    assert any(x["object_type"] == 4 for x in matches), "Expected actual Wwise event object"
    return {"source": str(path), "sha256": digest(data), "matches": matches}


def main():
    target = Target()
    exports = []
    for sym in target.pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = (sym.name or b"").decode("ascii")
        if name.startswith("?PostEvent@SoundEngine@AK@@") or name == "?GetIDFromString@SoundEngine@AK@@YAKPBD@Z":
            exports.append({"name": name, "rva": hex(sym.address),
                            "first_16_bytes": target.image[sym.address:sym.address + 16].hex()})
    assert event_id(EVENT) == 0xE8BB4D2C and len(exports) == 4
    report = {"grade": "STATIC", "executable_sha256": target.digest, "exports": exports,
              "event": EVENT, "event_id": hex(event_id(EVENT)),
              "post_event_abi": "x86 cdecl; eight DWORD-sized argument slots; decorated GameObjectID is I",
              "packages": [package(GAME / name) for name in ("Startup.upk", "Frontend.upk")],
              "sound_bank": bank(GAME / "SFX" / "A1E57F61.bnk", event_id(EVENT)),
              "limits": "Asset/export proof only. GPU candidate identities, all transition contexts and headset appearance remain unverified."}
    out = ROOT / "evidence" / "O1D-transitions-static.json"
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"receipt": str(out), "event_id": report["event_id"],
                      "movies_with_event": [len(p["movies_with_event"]) for p in report["packages"]]}))


if __name__ == "__main__":
    main()
