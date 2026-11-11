# USRP X310 / UBX-160 spectrum analyzer and Wi-Fi L-STF detection

C++20, UHD, GLFW/OpenGL, Dear ImGui/ImPlot and single-precision FFTW. Wi-Fi support
in this milestone is **L-STF candidate detection only**, at actual RX rate
20 MS/s (tolerance ±20 Hz). It does not decode OFDM symbols, L-LTF, HT-SIG or MAC.

## Hardware-independent build and tests

Requires Python 3 and a C++20 compiler. From the repository root:

```sh
python3 tools/build_wifi.py
build/wifi/wifi_detect recording.bin 4096
```

On Windows, run `python tools/build_wifi.py` from an **x64 Native Tools Command
Prompt for VS 2022**. This selects `cl /std:c++20 /EHsc`; executables have `.exe`
extensions. These targets require neither UHD, FFTW nor graphics libraries.
`--compiler` / `CXX` selects another compiler; `--output` selects the build directory.
The build script runs both C++ tests and the Python CLI integration test, propagating
compiler/test failures.

Offline input is interleaved **signed 16-bit little-endian I,Q**, four bytes per
complex sample, matching Windows/Linux x86 recordings. The recording must have
been captured at 20 MS/s. Block size is in samples; even size 1 is supported.
CSV output contains the candidate sample offset, correlation, coarse CFO in Hz,
normalized mean complex power and status. Malformed/truncated IQ and invalid block
sizes return a nonzero exit code. Offline and live paths use `LStfDetector`.

## GUI build

The original checkout contains vendor `.cpp` files but omits their headers.
Fetch matching pinned headers **outside the source tree**:

```sh
python3 tools/fetch_gui_headers.py
```

This supplies `build/include/imgui` from ImGui
`036bf939b6f8d74ad76bcf926b757c56e68c54ff` and ImPlot
`d65a2bef53d32502407de3a4be80f191e2f412d7`, including licenses. Source comparison
matches the bundled implementations after normalizing whitespace and their
`imgui/` include prefix. No vendor `.cpp` files are replaced.

Install compatible x64 UHD (including Boost headers/runtime), FFTW single precision,
GLFW and OpenGL development libraries with the same compiler/runtime ABI.
For MSVC, for example (substitute installed SDK paths):

```bat
python tools/build_app.py --include build/include --include build/include/imgui --include C:/UHD/include --include C:/Boost --include C:/fftw/include --include C:/glfw/include --lib C:/UHD/lib --lib C:/fftw/lib --lib C:/glfw/lib
```

Default MSVC library names are `uhd.lib`, `libfftw3f-3.lib`, `glfw3dll.lib` and
`opengl32.lib`. Repeat `--link NAME.lib` to override the complete library list
(e.g. for static GLFW, also supply its Win32 dependencies). Add UHD, FFTW, GLFW
and Boost DLL locations to `PATH`. Run `build/app/usrp.exe` from `build/app` so
`imgui.ini` and `recordings/` stay outside the sources.

For Linux, pass the applicable include/library roots to the same script. Example
using the published cloud environment's retained SDKs:

```sh
source /workspace/.cloud-setup/env.sh
python3 tools/fetch_gui_headers.py --cache /workspace/.cloud-setup --output /workspace/.cloud-setup/include
python3 tools/build_app.py --output /workspace/.cloud-setup/review-app \
  --include /workspace/.cloud-setup/include \
  --include /workspace/.cloud-setup/include/imgui \
  --include /workspace/.cloud-setup/sysroot/usr/include \
  --include /workspace/.cloud-setup/sysroot/usr/include/x86_64-linux-gnu \
  --lib /workspace/.cloud-setup/sysroot/usr/lib/x86_64-linux-gnu
```

The app retains FFTW for the spectrum and future OFDM processing. There is no
unused OFDM plan or speculative symbol-boundary calculation in the Wi-Fi stage.

## Live behavior and concurrency

1. Connect the X310 and select rate/frequency. The existing spectrum, averaging
   and raw-IQ recording controls remain available.
2. Before starting the stream, enable **Wi-Fi detection** at actual rate 20 MS/s
   to select `GetWiFiData()`. Otherwise reception uses the unchanged legacy
   `GetUSRPData()`. Stop a legacy stream before selecting Wi-Fi reception. In an
   already active Wi-Fi stream, the checkbox only changes detection worker state. The UI shows lifetime candidate count, latest score/CFO, recent
   events (last eight), known queue sample drops, queue overflows, and UHD errors.
3. Retuning stops and joins RX/processing before changing hardware settings, resets
   Wi-Fi state, then resumes if the stream was active. Leaving 20 MS/s disables
   detection. Stop/restart starts a new sample-index segment. Shutdown joins the
   connection, RX/processing and Wi-Fi workers before their state is destroyed.

There is one RX streamer and only one active receive method. `StartReception()`
selects either the unchanged legacy `GetUSRPData()` or the separate `GetWiFiData()`;
they never run concurrently. The following queue/partial-IQ guarantees apply to
`GetWiFiData()`. Its RX loop only handles metadata and copies samples into queues. Spectrum FFT and file I/O run in a processing worker;
Wi-Fi correlation runs in a separate worker. Each queue is bounded by block count
(64 spectrum/recording blocks; 32 Wi-Fi blocks), with blocks bounded by the RX
buffer size. RX never waits for queue space. Queue synchronization can briefly
wait on a mutex; it is not a lock-free hard-real-time implementation.

The legacy `GetUSRPData()` retains its original synchronous FFT/file I/O and
full-FFT-block receive requirement; it does not feed the Wi-Fi detector.
In Wi-Fi receive mode, valid short receives remain ordered. `IqAccumulator` retains incomplete FFT blocks
without padding. Queue overflow drops the newest block, counts its samples and
marks the next accepted block discontinuous. Wi-Fi resets correlation history on
that marker, segment change, offset gap or enable generation change. Queued data
from a previous enable generation cannot produce a new-generation detection.

UHD overflow/sequence/other non-timeout errors start a new segment because the
number of missing hardware samples is unknown. A timeout or a short successful
receive alone is not a discontinuity. Known software drops and hardware error
counts are reported separately; the hardware missing-sample count is not invented.
Segments are numbered within one device instance; offsets start at zero per
segment, and known Wi-Fi queue gaps retain their offset jumps.

Under sustained processing overload, spectrum/recording blocks may also be
rejected and counted; no finite queue guarantees lossless recording at arbitrary
rates/disk speeds. FFT accumulation is cleared and an active recording ends at
the first observed gap rather than stitching disjoint IQ together. A partial
trailing FFT block at stream end is not plotted, but is delivered to recording
and Wi-Fi. Disk-write/processing failures stop RX and are reported in the UI.

## Detector contract

For each 64-pair window, lag 16:

`R = sum(conj(x[n+k]) * x[n+k+16])`

`score = |R|² / (sum(|x[n+k]|²) * sum(|x[n+k+16]|²))`

Samples are normalized by 32768. Rolling products and both energies are updated
in O(1) per sample with fixed history, hence O(N) per input stream. Defaults:
score ≥0.75, power ≥1e-6, 32 qualifying windows to confirm a plateau, and 32
nonqualifying windows to rearm. Thresholds and plateau/release lengths are
configurable with `LStfConfig`. Each sample is visited once; chunk boundaries have
no special meaning.

An event's `sample_index` is the **start of the first qualifying correlation
window** in a confirmed plateau. It can precede the actual burst onset because the
window overlaps it. It is **not an exact packet start or OFDM symbol boundary**.
The event's score, power and CFO describe the confirmation window.
`CFO = arg(R) * Fs / (2π * 16)`; positive injected CFO gives positive estimates,
unambiguous only within ±625 kHz at 20 MS/s. No CFO compensation is performed yet.
Autocorrelation alone can also trigger on other periodic signals or a tone; a
candidate is not proof of a Wi-Fi packet. Long correlated plateaus are latched to
suppress duplicates until the release condition is met.

## Validation and limitations

Validated on Linux / GCC 14 / C++20:

- Full GUI build against UHD 4.8, FFTW 3.3.10 and GLFW 3.4; Xvfb launch and rendered
  `Vorot` window with no startup errors.
- Tests: independent IEEE legacy short-training subcarrier vector and direct
  inverse DFT, 27 amplitude/CFO/SNR combinations, positive/negative CFO (10 kHz
  tolerance with noise), candidate position against an independent brute-force
  window oracle and known burst bounds, eight chunk sizes including one sample,
  noise-only (200,000 samples), silence, multiple bursts, power gate and reset.
- Partial-RX accumulation preserves ordering/completeness of full FFT blocks;
  deterministic queue overflow counts and gap markers, no history bridging gaps,
  worker enable/rate gating/reset/shutdown.
- Explicit LE signed sc16 decoding, real-file CLI at four read sizes, injected
  −125 kHz CFO (100 Hz tolerance on clean IQ), malformed/truncated input rejection.
- AddressSanitizer + UndefinedBehaviorSanitizer: same C++ tests passed.

MSVC/Windows SDK linking, X310 connection, RF acquisition, retuning, recording on
hardware, sustained 20 MS/s throughput and overload behavior on real hardware have
**not** been validated in this Linux environment. Vendor ImPlot demo code emits
pre-existing enum-combination deprecation warnings. Hardware acceptance remains
necessary before claiming radio readiness.
