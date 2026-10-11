"""One-time isolated donor import. Does not edit or build the City project."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
src, dst = root / 'batman_city', root / 'batman_origins'
def copy_tree(a, b):
    shutil.copytree(a, b, dirs_exist_ok=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
copy_tree(src/'src/core', dst/'src/core')
for dep in ['imgui', 'OpenXR-SDK']:
    copy_tree(src/'third_party'/dep, dst/'third_party'/dep)
for file in ['CMakeLists.txt']:
    shutil.copy2(src/'third_party'/file, dst/'third_party'/file)
(dst/'src/game/origins').mkdir(parents=True, exist_ok=True)
for name in ['igame_adapter.h', 'adapter_registry.h']:
    shutil.copy2(src/'src/game'/name, dst/'src/game'/name)
for name in ['afw.cpp', 'afw.h', 'dlss_bridge.cpp', 'dlss_bridge.h']:
    shutil.copy2(src/'src/game/arkhamcity'/name, dst/'src/game/origins'/name)
copy_tree(src/'helper64', dst/'helper64')
for name in ['LICENSE-bioshock-vr.txt', 'LICENSE-luma-framework.md']:
    shutil.copy2(src/name, dst/name)
shutil.copy2(src/'src/proxy/xinput_proxy.cpp', dst/'src/proxy.cpp')
shutil.copy2(src/'src/proxy/xinput_proxy.def', dst/'src/proxy.def')
for base in [dst/'src/core', dst/'src/game', dst/'helper64']:
    for p in base.rglob('*'):
        if p.suffix not in ['.cpp', '.h', '.txt']: continue
        s=p.read_text(encoding='utf-8')
        for a,b in [('game/arkhamcity','game/origins'), ('bvr::ac::','bvr::bo::'),
                    ('Batman_City','Batman_Origins'),('batman_city','batman_origins'),
                    ('BatmanCityDLSS','BatmanOriginsDLSS'),('bc_dlss64','bo_dlss64')]:
            s=s.replace(a,b)
        p.write_text(s,encoding='utf-8')
p=dst/'src/proxy.cpp'
s=p.read_text().replace('batman_city','batman_origins')
s=s.replace('void LoadMod() {', '''void LoadMod() {
    // Ordinary Steam launches remain flat. The dedicated launcher supplies this token.
    const wchar_t* cmd = GetCommandLineW();
    const wchar_t* token = wcsstr(cmd, L"-bovr");
    if (!token || (token != cmd && token[-1] != L' ') ||
        (token[5] && token[5] != L' ' && token[5] != L'"')) return;''')
p.write_text(s,encoding='utf-8')
print('Imported City core, AFW, DLSS/helper and licenses into isolated Origins source.')
