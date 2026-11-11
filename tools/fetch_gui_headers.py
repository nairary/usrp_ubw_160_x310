#!/usr/bin/env python3
"""Supply omitted vendor headers outside source tree, at revisions matching bundled .cpp files."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--cache', type=Path, default=Path('build/dependencies'))
p.add_argument('--output', type=Path, default=Path('build/include'))
a = p.parse_args()
a.cache.mkdir(parents=True, exist_ok=True)
dest = a.output / 'imgui'
dest.mkdir(parents=True, exist_ok=True)
for name, commit, url in [
    ('imgui', '036bf939b6f8d74ad76bcf926b757c56e68c54ff', 'https://github.com/ocornut/imgui.git'),
    ('implot', 'd65a2bef53d32502407de3a4be80f191e2f412d7', 'https://github.com/epezent/implot.git'),
]:
    repo = a.cache / (name + '-upstream')
    if not repo.exists():
        subprocess.run(['git', 'init', str(repo)], check=True)
    prefix = ['git', '-C', str(repo)]
    if subprocess.run(prefix + ['cat-file', '-e', commit], stderr=subprocess.DEVNULL).returncode:
        subprocess.run(prefix + ['fetch', '--depth=1', url, commit], check=True)
    for f in subprocess.check_output(prefix + ['ls-tree', '-r', '--name-only', commit], text=True).splitlines():
        if f.endswith('.h') and ('/' not in f or f.startswith('backends/imgui_impl_')):
            (dest / Path(f).name).write_bytes(subprocess.check_output(prefix + ['show', commit + ':' + f]))
    (dest / (name + '-LICENSE.txt')).write_bytes(subprocess.check_output(prefix + ['show', commit + (':LICENSE.txt' if name == 'imgui' else ':LICENSE')]))
print('Pinned GUI headers:', a.output.resolve())
