# Batman Origins VR release package (PACKAGING_GUIDE.md, made like batman_city/tools/make_package.py): builds
# dist/BatmanOriginsVR-ArkhamOrigins.zip from one source of truth per file - the mod from build/Release, the DLSS helper
# from build64/Release, NVIDIA's DLSS library from the workspace SDK, the launcher scripts from tools/, the installer
# from install/, JJ's tested VR settings from his live settings.ini. Zip made with zipfile: forward slashes, no top
# folder (Windows' "Extract All" rejects backslash entries). The recorded bat transition and NVIDIA's DLSS library ship
# in the release zip only - never committed to the repository.
#   python tools/make_package.py [--settings "<settings.ini>"]
import argparse, hashlib, os, re, shutil, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VERSION = '0.1.3'
ap = argparse.ArgumentParser()
ap.add_argument('--settings', default=os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Batman_Origins', 'settings.ini'))
a = ap.parse_args()

dist = os.path.join(ROOT, 'dist')
stage = os.path.join(dist, 'BatmanOriginsVR')
zpath = os.path.join(dist, 'BatmanOriginsVR-ArkhamOrigins.zip')
if os.path.isdir(stage): shutil.rmtree(stage)
os.makedirs(os.path.join(stage, 'files'))

def put(src, rel):
    dst = os.path.join(stage, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.isdir(src): shutil.copytree(src, dst)
    else: shutil.copy2(src, dst)

inst = os.path.join(ROOT, 'install')
for f in ('Install-BatmanOriginsVR.bat', 'Install-BatmanOriginsVR.ps1', 'Uninstall-BatmanOriginsVR.bat',
          'Uninstall-BatmanOriginsVR.ps1', 'Batman Origins VR.bat', 'Batman Origins VR (SteamVR).bat'):
    put(os.path.join(inst, f), f)
rel = os.path.join(ROOT, 'build', 'Release')
for f in ('xinput1_3.dll', 'batman_origins.dll', 'bo_xr_config32.exe'):
    put(os.path.join(rel, f), f)
ov = os.path.join('files', 'OriginsVR')
put(os.path.join(HERE, 'Launch-VR.ps1'), os.path.join(ov, 'Launch-VR.ps1'))
put(os.path.join(HERE, 'Graphics-Profile.ps1'), os.path.join(ov, 'Graphics-Profile.ps1'))
put(os.path.join(ROOT, 'assets', 'bat-wipe.bin'), os.path.join(ov, 'bat-wipe.bin'))
for src, name in ((os.path.join(ROOT, 'LICENSE-bioshock-vr.txt'), 'LICENSE-bioshock-vr.txt'),
                  (os.path.join(ROOT, 'LICENSE-luma-framework.md'), 'LICENSE-luma-framework.md'),
                  (os.path.join(ROOT, 'third_party', 'minhook', 'LICENSE.txt'), 'LICENSE-MinHook.txt'),
                  (os.path.join(ROOT, 'third_party', 'imgui', 'LICENSE.txt'), 'LICENSE-ImGui.txt'),
                  (os.path.join(ROOT, 'third_party', 'OpenXR-SDK', 'LICENSE'), 'LICENSE-OpenXR.txt')):
    put(src, os.path.join(ov, name))
dl = os.path.join('files', 'BatmanOriginsDLSS')
put(os.path.join(ROOT, 'build64', 'Release', 'bo_dlss64.exe'), os.path.join(dl, 'bo_dlss64.exe'))
put(os.path.join(ROOT, '..', 'akvr_native', 'third_party', 'ngx', 'bin', 'nvngx_dlss.dll'), os.path.join(dl, 'nvngx_dlss.dll'))
put(a.settings, os.path.join('files', 'settings.ini'))
# FIRSTPERSON: new players start in third person whatever JJ last used (City SHIP_OVERRIDES first_person=0).
sp = os.path.join(stage, 'files', 'settings.ini')
st = open(sp, encoding='latin-1').read()
open(sp, 'w', encoding='latin-1', newline='').write(re.sub(r'(?ims)(^\[first_person\][^\[]*?^on=)1', r'\g<1>0', st))
open(os.path.join(stage, 'files', 'version.txt'), 'w', newline='\n').write(VERSION + '\n')
for f in ('README.md', 'LICENSE-bioshock-vr.txt', 'LICENSE-luma-framework.md', 'THIRD_PARTY_NOTICES.md'):
    put(os.path.join(ROOT, f), f)

# checks: batch files CRLF, the proxy is ours, the settings file is a tested one (no test switches), README is the player one
bad = []
for dp, _, fs in os.walk(stage):
    for f in fs:
        if f.lower().endswith('.bat'):
            b = open(os.path.join(dp, f), 'rb').read()
            if b.count(b'\n') != b.count(b'\r\n'): bad.append(f + ': not CRLF')
if b'[batman_origins-proxy]' not in open(os.path.join(stage, 'xinput1_3.dll'), 'rb').read(): bad.append('xinput1_3.dll is not ours')
st = open(os.path.join(stage, 'files', 'settings.ini'), encoding='utf-8', errors='replace').read()
for key in ('[view]', '[launch]', '[hud]', '[bats]'):
    if key not in st: bad.append(f'settings.ini lacks {key}')
if re.search(r'(?im)^(maprec|mapsafe|mapblit|eyeburst)\s*=\s*1', st): bad.append('settings.ini has a test switch on')
if 'Work in progress' in open(os.path.join(stage, 'README.md'), encoding='utf-8').read(): bad.append('README is the work-in-progress one')
if bad:
    print('REFUSED:', *bad, sep='\n  '); sys.exit(1)

h = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest()
if os.path.exists(zpath): os.remove(zpath)
with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for dp, _, fs in os.walk(stage):
        for f in sorted(fs):
            full = os.path.join(dp, f)
            arc = os.path.relpath(full, stage).replace(os.sep, '/')
            z.write(full, arc, zipfile.ZIP_STORED if f.lower().endswith(('.dll', '.exe')) else zipfile.ZIP_DEFLATED)
with zipfile.ZipFile(zpath) as z:
    assert z.testzip() is None
    names = z.namelist()
    assert not any('\\' in n for n in names), 'backslash in an entry name'
for n in names: print('  ', n)
print(f'{zpath}: {len(names)} files, {os.path.getsize(zpath) / 1e6:.1f} MB, sha256 {h(zpath)[:16]}')
