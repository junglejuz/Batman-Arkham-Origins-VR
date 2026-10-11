"""Read-only scouting for the fingerprinted retail Origins executable.

Requires pefile and capstone. Addresses are preferred-image VAs; save RVAs for
runtime use. Byte-pointer and E8 matches are leads, not proven Ghidra xrefs or
function boundaries. This utility never edits the executable or a Ghidra DB.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import capstone
import pefile

EXE = Path(r"E:\Games\Steam\steamapps\common\Batman Arkham Origins\SinglePlayer\Binaries\Win32\BatmanOrigins.exe")
SHA256 = "1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2"
COMMANDS = {
    "begin_execute": 0x665560,
    "begin_describe_a": 0x665570,
    "end_execute_a": 0x665590,
    "end_describe_a": 0x6655B0,
    "end_drawing_dispatch": 0x665610,
    "begin_describe_b": 0x6656F0,
    "end_execute_b": 0x665710,
    "end_describe_b": 0x665730,
}
TABLES = {"begin_a": 0x1492E98, "end_a": 0x1492EA8,
          "begin_b": 0x1492EC8, "end_b": 0x1492ED8}
VIEW_CANDIDATES = {"viewport_root_candidate": (0x67ED90, 900),
                   "calc_scene_view_candidate": (0x7D40F0, 1600),
                   "view_helper_identity_unresolved": (0x7D5080, 1800),
                   "scene_view_ctor_candidate": (0x9CFDC0, 1600)}


class Target:
    def __init__(self, path=EXE):
        raw = Path(path).read_bytes()
        self.digest = hashlib.sha256(raw).hexdigest()
        if self.digest != SHA256:
            raise ValueError(f"Unrecognized Origins build: {self.digest}; expected {SHA256}")
        self.pe = pefile.PE(data=raw)
        if self.pe.FILE_HEADER.Machine != 0x14C or self.pe.OPTIONAL_HEADER.ImageBase != 0x400000:
            raise ValueError("Expected x86 Origins at preferred image base 0x400000")
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.image = self.pe.get_memory_mapped_image()
        self.cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        section = next(s for s in self.pe.sections if s.Name.rstrip(b"\0") == b".text")
        self.text = (self.base + section.VirtualAddress,
                     self.base + section.VirtualAddress + section.Misc_VirtualSize)

    def matches(self, pattern, lo=None, hi=None):
        lo = 0 if lo is None else lo - self.base
        hi = len(self.image) if hi is None else hi - self.base
        at = self.image.find(pattern, lo, hi)
        while at >= 0:
            yield at + self.base
            at = self.image.find(pattern, at + 1, hi)

    def pointer_matches(self, va):
        return list(self.matches(struct.pack("<I", va)))

    def instructions(self, va, count=70, stop_at_ret=True):
        if not self.text[0] <= va < self.text[1]:
            raise ValueError(f"Address {va:#x} is outside executable .text")
        result = []
        for ins in self.cs.disasm(self.image[va-self.base:va-self.base+count*15], va):
            result.append({"va": f"0x{ins.address:08x}", "rva": f"0x{ins.address-self.base:08x}",
                           "bytes": ins.bytes.hex(), "instruction": f"{ins.mnemonic} {ins.op_str}".strip()})
            if len(result) >= count or (stop_at_ret and ins.mnemonic == "ret"):
                break
        return result

    def report(self):
        strings = {}
        for name in ("BeginDrawingCommand", "EndDrawingCommand", "UGameViewportClient",
                     "UGameViewportClientexecGetViewportSize", "UCanvasexecProject"):
            strings[name] = {}
            for encoding, terminator in (("ascii", b"\0"), ("utf-16le", b"\0\0")):
                hits = list(self.matches(name.encode(encoding) + terminator))
                strings[name][encoding] = [
                    {"va": f"0x{va:08x}", "pointer_byte_matches": [f"0x{x:08x}" for x in self.pointer_matches(va)]}
                    for va in hits]
        commands = {
            name: {"va": f"0x{va:08x}", "rva": f"0x{va-self.base:08x}",
                   "first_16_bytes": self.image[va-self.base:va-self.base+16].hex(),
                   "disassembly": self.instructions(va)}
            for name, va in COMMANDS.items()}
        tables = {
            name: {"va": f"0x{va:08x}", "rva": f"0x{va-self.base:08x}",
                   "slots": [f"0x{x:08x}" for x in struct.unpack_from("<III", self.image, va-self.base)],
                   "pointer_byte_matches": [f"0x{x:08x}" for x in self.pointer_matches(va)]}
            for name, va in TABLES.items()}
        return {"grade": "STATIC", "executable": str(EXE), "sha256": self.digest,
                "warning": "Linear disassembly and raw byte matches; no live caller counts or camera layouts proved.",
                "strings": strings, "commands": commands, "tables": tables}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=EXE)
    parser.add_argument("--out", type=Path, help="Write JSON evidence (otherwise print)")
    parser.add_argument("--dis", type=lambda s: int(s, 0), help="Disassemble from a known instruction boundary")
    parser.add_argument("--count", type=int, default=70)
    parser.add_argument("--refs", type=lambda s: int(s, 0), help="Find absolute pointer byte matches")
    parser.add_argument("--views", action="store_true", help="Save candidate-view disassembly (semantics unproved)")
    args = parser.parse_args()
    target = Target(args.exe)
    if args.dis is not None:
        result = target.instructions(args.dis, args.count)
    elif args.refs is not None:
        result = [f"0x{a:08x}" for a in target.pointer_matches(args.refs)]
    elif args.views:
        result = {name: {"va": f"0x{va:08x}", "rva": f"0x{va-target.base:08x}",
                         "grade": "STATIC candidate; semantics and complete ABI unproved",
                         "code": target.instructions(va, count)}
                  for name, (va, count) in VIEW_CANDIDATES.items()}
    else:
        result = target.report()
        result["executable"] = str(args.exe)
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"Saved {args.out} ({len(text)} characters); STATIC only")
    else:
        print(text)


if __name__ == "__main__":
    main()
