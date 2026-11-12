# USRP X310 / UBX-160 spectrum analyzer and Wi-Fi L-STF detection

C++20, UHD, GLFW/OpenGL, Dear ImGui/ImPlot and single-precision FFTW. Wi-Fi support includes **L-STF candidate detection and L-LTF timing/CFO synchronization**,
at actual RX rate 20 MS/s (tolerance ±20 Hz). It does not decode L-SIG, HT-SIG or MAC.

## Visual Studio 2026: native solution, no CMake

Open **`usrp_ubw_160_x310.sln`** by double-clicking it. This is a hand-maintained
MSBuild solution/project, not a CMake-generated wrapper. Building this solution
never invokes CMake, vcpkg, Python or a header-download script. The existing CMake
workflow below remains an independent alternative.

**Windows/MSVC compilation and F5 launch cannot be verified in this Linux
environment.** Project XML, configurations and source references were checked;
these checks do not establish a successful Windows build or hardware operation.
No reception or Wi-Fi algorithm source is changed by the native project setup.

### Prerequisites and dependencies

Install VS **2026**, **Desktop development with C++**, the **v145 x64/x86 MSVC**
toolset and a Windows 10/11 SDK. The project selects the latest installed 10.x SDK.
Both configurations are x64 and use C++20. If using another compatible installed
toolset, set the MSBuild property `UsrpPlatformToolset` (e.g. `v143` for VS 2022)
or retarget through Project Properties. Do not select Win32.

Use an installed **x64 UHD 4.x SDK**, and separately installed x64 FFTW and GLFW
DLL development packages. Supply Boost headers compatible with the UHD public
headers. Header versions and runtime DLLs belonging to UHD must match that SDK.
The restored ImGui/ImPlot headers and sources are already in `ext/imgui`.
OpenGL comes from the Windows SDK/graphics driver. `glad.c` is intentionally not
compiled: ImGui OpenGL3 already has its own loader.

Set these **user environment variables** to your actual SDK directories in
Windows Environment Variables, then restart Visual Studio:

| Variable | Default layout expected |
|---|---|
| `UHD_ROOT` | `include/uhd`, `lib/uhd.lib`, `bin/uhd.dll` |
| `BOOST_ROOT` | `boost/version.hpp`, optional `lib` and `bin` |
| `FFTW_ROOT` | `include/fftw3.h`, `lib/libfftw3f-3.lib`, `bin/libfftw3f-3.dll` |
| `GLFW_ROOT` | `include/GLFW/glfw3.h`, `lib/glfw3dll.lib`, `bin/glfw3.dll` |

Alternatively, copy `msbuild/Dependencies.local.props.example` to
`msbuild/Dependencies.local.props` and edit it for your machine. The local file
is ignored by Git; no machine-specific paths are committed. Roots can be supplied
as `UsrpUHDRoot`, `UsrpBoostRoot`, `UsrpFFTWRoot`, `UsrpGLFWRoot`. Any derived
`Usrp*IncludeDir`, `Usrp*LibDir`, `Usrp*BinDir` or library-name property can be
overridden directly. See `msbuild/Dependencies.props` for all defaults. The local
file is imported before derived defaults, so changing a root updates its paths.

The example covers common archive layouts: GLFW import libraries/DLLs under
`lib-vc2022`, Boost headers directly under the Boost root, and FFTW headers/DLLs
at the archive root. For the official FFTW prebuilt archive, generate its x64
single-precision import library if it is not supplied, in VS Developer PowerShell:

```powershell
# Run in the FFTW archive directory containing libfftw3f-3.def:
lib.exe /machine:x64 /def:libfftw3f-3.def /out:libfftw3f-3.lib
```

Do not link double-precision `libfftw3-3.lib` in place of `libfftw3f-3.lib`.
For an SDK using `libuhd.lib` / `libuhd.dll`, override `UsrpUHDLibrary` and the
UHD filename in `UsrpRequiredDlls`, as shown in the example.

The current app includes Boost format/program_options/thread headers but does not
call out-of-line Boost library APIs. The project defines `BOOST_ALL_NO_LIB` to
avoid accidental compiler/version/debug-tag autolinking. `UsrpBoostLibDir` and
`UsrpBoostLibraries` remain available for explicit matching Boost libraries if
needed by a different SDK. UHD's own Boost runtime DLLs are still required; use
those shipped with that UHD build, not arbitrary substitutes.

### Build and F5

1. Double-click `usrp_ubw_160_x310.sln` and select **x64 / Release**.
2. If necessary, right-click `usrp_ubw_160_x310` and **Set as Startup Project**.
3. **Build Solution**. Missing headers/import libraries report the exact property
   to configure. No source files or library paths need adding to the project.
4. Press **F5**. The debugger working directory is the executable output directory:
   `build/msbuild/x64/Release` (or `Debug`). Recordings and ImGui settings stay there.

Equivalent commands in VS 2026 Developer PowerShell:

```powershell
msbuild usrp_ubw_160_x310.sln /m /p:Configuration=Release /p:Platform=x64
msbuild usrp_ubw_160_x310.sln /m /p:Configuration=Debug /p:Platform=x64
```

**Debug intentionally uses `/MD`, `_ITERATOR_DEBUG_LEVEL=0`, debug symbols and
no optimization** so its C++ ABI matches a prebuilt Release UHD SDK. It is a
configuration for debugging application code against release dependencies, not
an `/MDd` SDK configuration. Release uses `/MD`, optimization and symbols too.
Do not enable checked STL or switch to `/MDd` unless all C++ dependencies are
rebuilt and configured consistently. The Console subsystem keeps UHD diagnostics
visible. No MFC/ATL dependency or additional `main()` from tools/tests is included.

### DLL staging and hardware

After every Build, native MSBuild tasks copy `*.dll` from the configured UHD,
Boost, FFTW and GLFW bin directories into the output directory. Add directories
containing other DLLs shipped with your UHD SDK using `UsrpExtraRuntimeDirs`
(semicolon-separated). Only the named directories are searched, not recursively.
The project checks for `UsrpRequiredDlls` after copying. Conflicting duplicate DLL
filenames fail staging instead of silently choosing a version. Keep a single
compatible provider for each DLL name. After changing dependency versions, remove
old output DLLs / delete the configuration's output directory before rebuilding.

This is directory-based staging, not an automatic transitive import scanner:
ensure each SDK runtime directory contains its complete matching dependency set.
If F5 reports a further missing DLL, inspect the executable/UHD DLL with
`dumpbin /dependents` in the VS Developer shell and add that SDK's runtime directory.
Install the Microsoft VC++ 2015–2026 **x64** runtime supported by the toolset on
machines without Visual Studio. Do not copy Windows system DLLs or download
individual DLLs from third-party sites. Error `0xc000007b` usually warrants checking
for a mixed x86/x64 DLL set. Do not launch a copied `.exe` without its runtime DLLs.

Set `UHD_IMAGES_DIR` if the installed UHD cannot find the matching FPGA images.
Use the installed UHD image downloader and utilities; no image flashing or network
configuration is performed by this project. Verify X310 NIC/subnet/firewall and
run `uhd_find_devices --args="type=x300"` / `uhd_usrp_probe --args="type=x300"`
from the UHD SDK before application testing. Then use **Turn on receiver** in the
app. Native solution validation does not establish hardware readiness.

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
unambiguous only within ±625 kHz at 20 MS/s. `LStfDetector` reports CFO without
altering input; the worker applies coarse CFO correction during L-LTF matching.
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

## L-LTF synchronization in the Wi-Fi worker

`WiFiDemodulator::run()` retains normalized IQ in a bounded history independently
of input chunks. After an L-STF candidate it searches the first useful legacy LTF
in `[candidate_index + 128, candidate_index + 288]`, rather than assuming a symbol
boundary from the autocorrelation crossing. Search completes once both 64-sample
symbols are available for the entire window (through `candidate_index + 416`).
Internal chunks are at most 256 samples; history is trimmed to 1024 samples.
An incomplete candidate waits for more continuous IQ; a discontinuity, segment or
enable-generation change clears history and pending candidates.

The known IEEE L-LTF reference is correlated after coarse CFO correction. Both
64-sample matches must pass 0.55 normalized squared correlation, their repetition
must pass 0.75, and the preceding 32-sample cyclic extension must pass 0.65.
The best paired match supplies the first useful LTF index. This is a strongest
matched-path timing estimate; in multipath it is not necessarily the earliest
physical arrival. The nominal L-STF origin is inferred by subtracting 192 samples.
The residual CFO uses the phase between the two LTF copies after removing the
coarse phase advance: `fine = arg(R * exp(-j*2*pi*coarse*64/Fs))*Fs/(2*pi*64)`.
`total_cfo_hz = coarse_cfo_hz + fine_cfo_hz`; fine CFO is unambiguous within
±156.25 kHz of the coarse estimate at 20 MS/s.

Results are published under a short lock in `Snapshot::recent_sync` (last eight):
segment, STF candidate index, first LTF index, inferred packet start, match score,
coarse/residual/total CFO. `synchronized_packets` and `rejected_candidates` are
lifetime counters; recent synchronization results clear on enable changes. The
GUI shows the last timing result and total CFO. Correlation runs outside the UI
and state mutex. No corrected OFDM stream, channel estimate or L-SIG decoder is
produced yet; CFO compensation here is used for LTF matching and estimation.
The standalone `wifi_detect` CLI remains an L-STF-only detector.

Deterministic worker tests cover known timing/CFO, positive and negative CFO,
12 dB and clean fixtures, single-sample through whole-buffer input, consecutive
packets, segment-start packets, rejected tone/STF-only/invalid-LTF inputs and a
split LTF across a discontinuity. These are synthetic AWGN/single-path checks;
real captures, severe multipath, Windows/MSVC and X310 throughput remain unverified.
