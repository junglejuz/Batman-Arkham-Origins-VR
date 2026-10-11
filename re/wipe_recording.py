"""Validate the user's private City bat-wipe recording for local Origins replay.
No game or donor files are written. This is animation data, not transferable
engine addresses or shader identities. Do not publish the captured game asset.
"""
import hashlib
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw

SOURCE = Path.home() / 'AppData/Local/Batman_City/menu_wipe.bin'
ROOT = Path(__file__).resolve().parents[1]


def main():
    data = SOURCE.read_bytes()
    magic, version, width, height, count = struct.unpack_from('<5I', data)
    assert magic == 0x50574342 and version == 2
    assert 8 <= count <= 160 and 0 < width <= 4096 and 0 < height <= 4096
    times = struct.unpack_from(f'<{count}I', data, 20)
    assert all(a < b for a, b in zip(times, times[1:])) and times[-1] < 5000
    pos = 20 + count * 4
    frames = []
    stats = []
    for i in range(count):
        size = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        assert size % 2 == 0 and pos + size <= len(data)
        pixels = bytearray()
        for at in range(pos, pos + size, 2):
            n, v = data[at:at + 2]
            assert n > 0 and len(pixels) + n <= width * height
            pixels.extend(bytes([v]) * n)
        assert len(pixels) == width * height
        pos += size
        frames.append(bytes(pixels))
        stats.append({'frame': i, 'ms': times[i], 'mean_coverage': sum(pixels) / (255 * len(pixels))})
    assert pos == len(data)
    asset = ROOT / 'assets/bat-wipe.bin'
    asset.parent.mkdir(exist_ok=True)
    if asset.exists():
        assert asset.read_bytes() == data
    else:
        asset.write_bytes(data)
    indices = [0, count // 5, count * 2 // 5, count * 3 // 5, count * 4 // 5, count - 1]
    sheet = Image.new('RGB', (720, 2 * 280), 'white')
    draw = ImageDraw.Draw(sheet)
    for n, i in enumerate(indices):
        frame = Image.frombytes('L', (width, height), frames[i]).point(lambda v: 255-v).resize((230,245))
        x, y = (n % 3)*240, (n // 3)*280
        sheet.paste(frame, (x,y+25))
        draw.text((x+4,y+4), f'{i}: {times[i]} ms', fill='black')
    image = ROOT / 're/evidence/O1F-wipe-contact.png'
    sheet.save(image)
    report = {'grade': 'STATIC asset validation', 'source': str(SOURCE), 'asset': str(asset),
              'sha256': hashlib.sha256(data).hexdigest(), 'version': version,
              'width': width, 'height': height, 'frames': count, 'times_ms': list(times),
              'coverage': stats, 'contact_sheet': str(image),
              'boundary': 'Private local captured game animation; no publication or redistribution.'}
    out = ROOT / 're/evidence/O1F-wipe-recording.json'
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k:report[k] for k in ['sha256','width','height','frames']}))
    print('first/last coverage:', stats[0], stats[-1])


if __name__ == '__main__':
    main()
