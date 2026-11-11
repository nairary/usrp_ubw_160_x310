"""Exercise actual files and CLI, using a direct IEEE L-STF inverse DFT reference."""
import cmath
import csv
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
tones = [-24, -20, -16, -12, -8, -4, 4, 8, 12, 16, 20, 24]
signs = [1, -1, 1, -1, -1, 1, -1, -1, 1, 1, 1, 1]
stf = [sum(s * (1+1j) * cmath.exp(2j*cmath.pi*k*n/64) for k, s in zip(tones, signs)) for n in range(160)]
rms = (sum(abs(x)**2 for x in stf)/160)**.5
samples = [0j]*300 + [x/rms*.2*cmath.exp(2j*cmath.pi*(-125000)*n/20e6) for n, x in enumerate(stf)] + [0j]*300
with tempfile.TemporaryDirectory() as folder:
    recording = Path(folder)/'reference.bin'
    recording.write_bytes(b''.join(struct.pack('<hh', round(x.real*32768), round(x.imag*32768)) for x in samples))
    expected = None
    for block in [1, 3, 79, 4096]:
        result = subprocess.run([str(binary), str(recording), str(block)], check=True, capture_output=True, text=True).stdout
        if expected is None:
            expected = result
        if result != expected:
            raise AssertionError('CLI event depends on read block size')
    rows = list(csv.DictReader(io.StringIO(expected)))
    if len(rows) != 1 or abs(float(rows[0]['cfo_hz'])+125000) > 100:
        raise AssertionError('CLI detection/CFO mismatch')
    recording.write_bytes(recording.read_bytes()+b'x')
    if subprocess.run([str(binary), str(recording)], capture_output=True).returncode == 0:
        raise AssertionError('truncated recording accepted')
    if subprocess.run([str(binary), str(recording), '0'], capture_output=True).returncode == 0:
        raise AssertionError('zero block size accepted')
print('offline CLI: PASS (real sc16 file, four read sizes, CFO, malformed input)')
