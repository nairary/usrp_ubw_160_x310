# USRP X310 / UBX-160 spectrum analyzer and Wi-Fi L-STF detection

C++20, UHD, GLFW/OpenGL, Dear ImGui/ImPlot and single-precision FFTW. Wi-Fi support
in this milestone is **L-STF candidate detection only**, at actual RX rate
20 MS/s (tolerance ±20 Hz). It does not decode OFDM symbols, L-LTF, HT-SIG or MAC.

## Visual Studio 2022 / Windows x64 (CMake)

**Windows/MSVC build and DLL staging have not been tested in the current Linux
cloud environment.** Linux CMake builds/tests validate the source lists and
restored headers, but cannot validate Windows ABI, MSVC linking, DLL discovery or
X310 hardware operation. Reception and Wi-Fi algorithms are unchanged by this
build setup.

### 1. Install prerequisites

- Visual Studio 2022, **Desktop development with C++**, MSVC v143 x64/x86 tools,
  Windows 10/11 SDK, **C++ CMake tools for Windows**. Use a current VS 2022 release
  with CMake **3.24 or newer** (presets schema 5).
- Git for Windows. Python 3 is optional for the offline CLI integration test and
  UHD image-downloader scripts; the C++ test always runs.
- Official **Windows x64 UHD SDK**, including development headers, import library,
  runtime DLLs and utilities. This project uses UHD 4.8 APIs; use a compatible
  UHD 4.x SDK and matching FPGA images. The SDK is used in place, not rebuilt.
  Default lookup is `%ProgramFiles%/UHD`; a custom install uses `UHD_ROOT` below.
- Microsoft Visual C++ 2015–2022 Redistributable **x64** on machines running the
  app without Visual Studio. Do not use 32-bit libraries.

### 2. Prepare vcpkg and environment

In PowerShell (replace paths with your install locations):

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/src/vcpkg
# Match the manifest baseline for a reproducible initial installation:
git -C C:/src/vcpkg checkout 2ec0f1747d8186a5c9babe276e2ad57cd2b12d2a
& C:/src/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = "C:/src/vcpkg"
$env:UHD_ROOT = "C:/Program Files/UHD"
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $env:VCPKG_ROOT, "User")
[Environment]::SetEnvironmentVariable("UHD_ROOT", $env:UHD_ROOT, "User")
```

Restart Visual Studio after changing persistent environment variables. Internet
access is needed for vcpkg's first manifest restore. The manifest installs Boost
format/program_options, FFTW3 (the app links **single precision fftw3f**) and GLFW
for **x64-windows**. OpenGL is provided by the Windows SDK/graphics driver.
There is no UHD port in the pinned registry: use the installed SDK. Do not install
a second UHD or add hand-written `.lib`/include lists to the Visual Studio project.

### 3. Open, build, run

**Open Folder** in VS 2022, select the repository root, then choose the
`Windows x64 / VS 2022 / vcpkg` configure preset. If VS has legacy CMake integration
enabled, enable **Use CMakePresets.json** in its CMake options and reopen the folder.
Select **Release** (or **RelWithDebInfo**) and startup target **usrp.exe**. Build
and press F5. CMake lists all sources, restores dependencies, links the targets and
stages non-system runtime DLLs next to the executable automatically. No manual
adding of `.cpp`, include directories or library directories is needed.

Alternatively, in a VS 2022 Developer PowerShell with the same environment:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-release
ctest --preset windows-release
# Open the generated solution if preferred:
start build/vs2022/usrp_ubw_160_x310.sln
# Or run directly, with generated data kept in the build tree:
Push-Location build/vs2022/Release
./usrp.exe
Pop-Location
```

CMake sets `usrp` as the generated solution's startup project and its executable
directory as debugger working directory. This keeps `imgui.ini` and `recordings/`
in the build tree. Presets expose Release/RelWithDebInfo, using `/MD` to match
release UHD and the `x64-windows` dynamic CRT. RelWithDebInfo provides application
symbols while using the same release SDKs. Do not mix `/MDd`, static-triplet Boost,
x86 DLLs or unrelated UHD DLL versions into this configuration.

### UHD runtime and X310

The SDK root must contain `include/uhd`, `lib/uhd.lib` (or `libuhd.lib`) and
`bin/uhd.dll` (or `libuhd.dll`). For a nonstandard SDK layout, set the advanced
CMake cache entries `UHD_INCLUDE_DIR`, `UHD_LIBRARY`, `UHD_RUNTIME_DLL` once; all
application/library source paths still come from CMake.

After linking, a CMake `dumpbin` dependency scan copies UHD, vcpkg and transitive
SDK DLLs beside `usrp.exe`, excluding Windows system/API-set DLLs. The SDK's own
`bin` and vcpkg release `bin` are searched automatically. If UHD's matching Boost
or other runtime DLLs are in a separate directory, set the CMake cache variable
`USRP_RUNTIME_DIRS` (a semicolon-separated directory list), for example:

```powershell
cmake --preset windows-vs2022 '-DUSRP_RUNTIME_DIRS=C:/UHD-runtime/bin;C:/UHD-Boost/lib'
cmake --build --preset windows-release
```

Use the DLLs shipped/built with that **same UHD SDK**. vcpkg Boost supplies
application headers/libraries; it does not replace version-specific dependencies
of an externally built UHD DLL. An unresolved runtime dependency deliberately
fails the post-build step with its name. Fix the SDK runtime directory and rebuild;
do not download arbitrary DLLs. If launching fails with `0xc000007b`, check x64
architecture throughout. For `DLL not found`, also verify the x64 VC++ runtime.
Run from the build output directory or use F5, not a copied executable alone.

FPGA image data is not a DLL and is not copied beside the app. Use the UHD SDK's
image downloader as documented by that SDK, and point `UHD_IMAGES_DIR` at the
matching installed/downloaded image directory if UHD cannot locate it. Install
images matching the UHD runtime and X310 FPGA/network configuration; firmware
flashing is not performed by this build setup. Configure the PC NIC/subnet and
firewall according to Ettus's X3xx guide. From the SDK utilities directory run:

```powershell
./uhd_find_devices.exe --args="type=x300"
./uhd_usrp_probe.exe --args="type=x300"
```

Then start `usrp.exe` and click **Turn on receiver**. The current application
searches with `type=x300`; no hardware-specific reception code was changed. Use
matching image/runtime versions and validate connectivity before debugging the
application. Image download, NIC configuration and actual X310 capture must be
verified on the Windows hardware host.

## Dependency/source layout

| Dependency | Build source |
|---|---|
| UHD | Installed x64 SDK, imported `UHD::uhd`; root via `UHD_ROOT`/`USRP_UHD_ROOT` |
| Boost | vcpkg `boost-format`, `boost-program-options`; `Boost::program_options` |
| FFTW3 | vcpkg `fftw3`; `FFTW3::fftw3f` (single precision) |
| GLFW | vcpkg `glfw3`; target `glfw` |
| OpenGL | System/Windows SDK `OpenGL::GL` |
| Dear ImGui docking / ImPlot | Bundled implementations and restored pinned headers; see [versions and licenses](ext/imgui/VERSIONS.md) |
| GLAD | Root `glad.c` is unused and excluded; ImGui's OpenGL3 backend has its own loader |

CMake explicitly lists vendor sources once. Do **not** additionally install/link
vcpkg imgui/implot, compile upstream implementations, or add `glad.c` to the
application. Existing `imgui/...` include paths are supported by exported target
include directories. The bundled docking API and ImPlotSpec API require these
matched revisions, not arbitrary release headers. No header-fetch script is
needed for CMake: all required headers/licenses are committed.

## Hardware-independent build and tests

With CMake 3.24+, Ninja and a C++20 compiler on Linux or Windows:

```sh
cmake --preset offline
cmake --build --preset offline
ctest --preset offline
build/offline/wifi_detect recording.bin 4096
```

Use `.exe` on Windows and a VS Developer shell. This preset needs no UHD, vcpkg,
FFTW or GUI libraries. CTest runs the C++ detector tests and, when Python is
available, the real-file offline CLI integration test. For older manual compiler
workflows, `python tools/build_wifi.py` remains available.

Offline input is interleaved signed 16-bit little-endian I,Q, four bytes per
complex sample, captured at 20 MS/s. Block size is in samples, including size 1.
CSV includes candidate sample offset, correlation, coarse CFO, power and status.
Malformed/truncated IQ or invalid block sizes return a nonzero exit code.
Offline and live Wi-Fi paths use the same `LStfDetector`.

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

Validated on Linux / GCC 14 / C++20 (including the CMake build):

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
