#!/usr/bin/env python3
"""Build hardware-independent detector tools with GCC/Clang or MSVC (developer shell)."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=Path('build/wifi'))
parser.add_argument('--compiler', default=os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
compiler = shutil.which(args.compiler)
if not compiler:
    parser.error('compiler unavailable; use a C++20 toolchain (MSVC developer shell on Windows)')
msvc = Path(compiler).stem.lower() == 'cl'
common = [root / name for name in ('LStfDetector.cpp', 'WiFiDemodulator.cpp', 'Sc16Reader.cpp')]
for target, source in [('wifi_tests', 'tests/wifi_tests.cpp'), ('wifi_detect', 'tools/wifi_detect.cpp')]:
    binary = output / (target + ('.exe' if os.name == 'nt' else ''))
    sources = [str(p) for p in common + [root / source]]
    if msvc:
        command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/W4', '/O2', '/I' + str(root), *sources, '/Fe:' + str(binary)]
    else:
        command = [compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-pthread', '-I' + str(root), *sources, '-o', str(binary)]
    subprocess.run(command, cwd=output, check=True)
subprocess.run([str(output / ('wifi_tests.exe' if os.name == 'nt' else 'wifi_tests'))], cwd=output, check=True)

subprocess.run([sys.executable, str(root / "tests/offline_cli_test.py"), str(output / ("wifi_detect.exe" if os.name == "nt" else "wifi_detect"))], check=True)
