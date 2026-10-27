"""AIMFIND (2026-10-27): find the fields that carry the game camera's rotation in an Origins aimfinder_*.bin recording.

Recording (first_person.cpp find_sample): "AMDF", object count N, bytes per object B, then N x (controller field, pointer);
then samples of (t, camera yaw deg, camera pitch deg) + N x B bytes. A field is a candidate when, over the samples, it follows
the camera's yaw (or pitch) within a tolerance in >= 90 % of them while the camera turned enough to rule out chance.
Interpretations: int32 UE3 rotator units (65536 / turn), float degrees, float radians.

usage: python aimfinder.py <aimfinder_*.bin> [--tol 2.0]
"""
import struct
import sys

import numpy as np


def load(path):
    data = open(path, 'rb').read()
    magic, n, nbytes = struct.unpack_from('<4sII', data, 0)
    if magic != b'AMDF':
        raise SystemExit('not an aimfinder recording')
    off = 12
    objs = []
    for _ in range(n):
        field, ptr = struct.unpack_from('<II', data, off)
        objs.append((field, ptr))
        off += 8
    rec = 12 + n * nbytes
    count = (len(data) - off) // rec
    cam = np.zeros((count, 3), np.float32)
    mem = np.zeros((count, n, nbytes), np.uint8)
    for s in range(count):
        base = off + s * rec
        cam[s] = struct.unpack_from('<fff', data, base)
        mem[s] = np.frombuffer(data, np.uint8, n * nbytes, base + 12).reshape(n, nbytes)
    return objs, cam, mem


def wrap180(a):
    return (a + 180.0) % 360.0 - 180.0


def search(objs, cam, mem, tol):
    yaw, pitch = cam[:, 1], cam[:, 2]
    spread = {'yaw': float(np.ptp(np.unwrap(np.radians(yaw)))) * 57.29578, 'pitch': float(np.ptp(pitch))}
    print(f'{len(cam)} samples, {len(objs)} objects; camera turned: yaw {spread["yaw"]:.0f} deg, pitch {spread["pitch"]:.0f} deg')
    if spread['yaw'] < 60:
        print('WARNING: the camera hardly turned - candidates are not reliable')
    ints = mem.view('<i4')            # (samples, objects, B/4)
    floats = mem.view('<f4')
    found = []
    for kind, values in (('rotator', (ints.astype(np.int64) & 0xFFFF).astype(np.float64)),
                         ('degrees', floats.astype(np.float64)),
                         ('radians', floats.astype(np.float64))):
        if kind == 'rotator':
            deg = np.where(values >= 32768, values - 65536, values) * 360.0 / 65536.0
        elif kind == 'degrees':
            deg = values
        else:
            deg = np.degrees(values)
        with np.errstate(invalid='ignore'):
            for axis, ref in (('yaw', yaw), ('pitch', pitch)):
                d = np.abs(wrap180(deg - ref[:, None, None]))
                ok = np.nanmean(d < tol, axis=0)          # (objects, fields)
                moved = np.nanmax(deg, axis=0) - np.nanmin(deg, axis=0)
                for o, f in zip(*np.nonzero((ok >= 0.9) & (moved > 20))):
                    found.append((float(ok[o, f]), axis, kind, int(o), int(f) * 4))
    found.sort(key=lambda x: (-x[0], x[1], x[3], x[4]))
    if not found:
        print('no field follows the camera')
    for score, axis, kind, o, off in found[:60]:
        field, ptr = objs[o]
        where = 'controller' if field == 0xFFFFFFFF else f'object at controller +0x{field:X} ({ptr:08X})'
        print(f'{score * 100:5.1f} %  {axis:5s} {kind:7s}  {where} +0x{off:X}')
    return found


if __name__ == '__main__':
    tol = float(sys.argv[sys.argv.index('--tol') + 1]) if '--tol' in sys.argv else 2.0
    search(*load(sys.argv[1]), tol)
