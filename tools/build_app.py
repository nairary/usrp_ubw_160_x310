#!/usr/bin/env python3
"""Build GUI with externally supplied UHD/FFTW/GLFW and matching vendor headers."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--compiler', default=os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++'))
p.add_argument('--output', type=Path, default=Path('build/app'))
p.add_argument('--include', action='append', type=Path, required=True, help='include roots (repeatable)')
p.add_argument('--lib', action='append', type=Path, default=[])
p.add_argument('--link', action='append', default=[], help='override library names; on MSVC include .lib suffix')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
output = a.output.resolve()
output.mkdir(parents=True, exist_ok=True)
includes = [d.resolve() for d in a.include]
libs = [d.resolve() for d in a.lib]
sources = [str(root / f) for f in ['main.cpp', 'Application.cpp', 'USRPDevice.cpp', 'ReaTimeFFT.cpp', 'WiFiDemodulator.cpp', 'LStfDetector.cpp', 'PlotFunctions.cpp']]
sources += [str(f) for f in sorted((root / 'ext/imgui').glob('*.cpp'))]
msvc = Path(a.compiler).stem.lower() == 'cl'
if msvc:
    links = a.link or ['uhd.lib', 'libfftw3f-3.lib', 'glfw3dll.lib', 'opengl32.lib']
    command = [a.compiler, '/nologo', '/std:c++20', '/EHsc', '/W4', '/MD', '/DNOMINMAX', *['/I'+str(i) for i in includes], *sources, '/Fe:usrp.exe', '/link', *['/LIBPATH:'+str(d) for d in libs], *links]
else:
    links = a.link or ['uhd', 'fftw3f', 'glfw', ':libGL.so.1', 'dl']
    command = [a.compiler, '-std=c++20', '-O0', '-pthread', *['-I'+str(i) for i in includes], *sources, *['-L'+str(d) for d in libs], *['-Wl,-rpath-link,'+str(d) for d in libs], *['-l'+n for n in links], '-o', 'usrp']
subprocess.run(command, cwd=output, check=True)
