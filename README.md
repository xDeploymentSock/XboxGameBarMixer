# Software Fuser

A transparent Xbox Game Bar widget that receives a Sunshine/Moonlight video feed from another PC and overlays its HUD on the local display. Video renders inside the UWP widget using D3D11, with hardware FFmpeg decoding and a view-only Moonlight transport.

**Current development checkpoint: 0.2.1.6.** The installed color fix was accepted in live use. Black removal keeps surviving decoded colors opaque; pairing, pinning, transparency, click-through and live video have been confirmed.

The target is **2560 x 1440 at 240 requested FPS** on Windows 11 x64 over wired LAN. Measured live pipeline rates exceeded the accepted 200 FPS milestone. Receive/decode/Present counts do not establish 240 distinct displayed frames or capture-to-screen latency. See the [validation record](docs/VALIDATION.md) for evidence and limits.

## Use the widget

1. Build and explicitly install the local development package using the [build guide](docs/BUILD-LATER.md). Builds do not install or start it.
2. Press **Win+G** and open **Software Fuser** from the widget menu.
3. On **Connect**, enter your Sunshine hostname or IP, choose **Pair with Sunshine**, and submit the displayed PIN on the source PC. Existing pairing survives package updates.
4. Click **Refresh apps**, select **Desktop** or the intended application, and click **Connect**. Desktop selection is validated against Sunshine's current application mapping; another active application is preserved.
5. On **HUD**, choose **Black** and **Remove black only** for opaque retained colors. **Remove near-black noise** also deletes very dark pixels. Use **Crisp HUD** scaling for thin text if preferred.
6. On **Layout**, use **Apply video fit** to fill the usable area above the adjustable taskbar reservation. This scales the complete feed and permits vertical compression. Widget dimensions and negotiated stream dimensions are separate.
7. Pin the widget, enable Game Bar click-through, and close Game Bar to use the overlay. **Disconnect** stops this client's stream.

The source HUD should use a reserved black, green or magenta background. **Draw test pattern** submits one local diagnostic frame. Local mouse/keyboard input stays on the receiving PC; the client forwards no input and plays no audio. See [Desktop launch behavior](docs/DESKTOP-LAUNCH.md) and [the menu guide](docs/WIDGET-MENU.md).

## Placement and image quality

Game Bar controls the widget's outer frame and may reject size or full-screen requests. **Fit my monitor**, typed **Apply dimensions** and **Reset widget position** are explicit actions; reopening and pinning do not trigger app fitting. Manual placement may be necessary. Automatic simultaneous top-edge/taskbar coverage remains unresolved on the tested host and is being investigated separately.

Main scales video into the available client area above the taskbar. This keeps the complete source frame visible while host coverage is investigated. Black removal preserves decoded RGB values; compression, chroma subsampling and scaling can still affect source text edges. Video opacity is independent of Game Bar opacity unless explicitly enabled in HUD fine tuning.

## Build and check

Use Visual Studio 2022 with v143 C++, UWP C++ tools and Windows SDK 10.0.26100.0 for the widget. The [build guide](docs/BUILD-LATER.md) covers pinned dependencies, GPU/control tests and explicit unsigned development deployment.

The portable C++20 core needs CMake 3.24+, a C++20 compiler, Git and Python 3.10+ for repository checks:

```text
python tools/audit_repository.py
cmake -S . -B build/core -DFUSER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

GitHub Actions runs these CPU-only checks on Windows and Linux. UWP builds, hardware decoding, live Game Bar behavior and monitor measurements require the separate Windows validation procedures.

## Documentation

| Topic | Guide |
| --- | --- |
| Design and rendering boundaries | [Architecture](docs/ARCHITECTURE.md) |
| Setup, tests and explicit deployment | [Build guide](docs/BUILD-LATER.md) |
| Black removal and natural colors | [Black cleanup](docs/BLACK-CLEANUP.md) |
| Feed fitting and placement | [Video fit](docs/VIDEO-FIT.md) |
| Text quality and receiver timing | [HUD quality/latency](docs/HUD-QUALITY-LATENCY.md), [receiver performance](docs/RECEIVER-PERFORMANCE.md) |
| Evidence and future work | [Validation](docs/VALIDATION.md), [roadmap](docs/ROADMAP.md) |
| Repository maintenance | [Contributing](CONTRIBUTING.md), [recommended skills](docs/REPOSITORY-SKILLS.md) |
| Release notes and older checkpoints | [Changelog](CHANGELOG.md), [development history](docs/DEVELOPMENT-HISTORY.md) |
| Upstream references and notices | [References](docs/REFERENCES.md), [third-party notices](THIRD-PARTY-NOTICES.md) |

## Repository layout

- `native/widget`: C++/WinRT, XAML and Game Bar package.
- `native/core`: portable settings, frame contracts and latest-frame mailbox.
- `native/windows` / `native/shaders`: D3D11 rendering and hardware decoding.
- `native/streaming`: Sunshine control and stream orchestration.
- `tests`: owned source HUD, core, GPU, decoder and control fixtures.
- `tools` / `docs`: build, deployment, measurement and documentation.

See [contributing](CONTRIBUTING.md) for checkpoint checks. Packages, logs, private machine/network records, certificates and pairing credentials stay out of Git. Protected pairing remains in app-local storage. The repository audit checks common accidental publication patterns; review the diff before pushing. Dependency license notices are retained in `third_party/licenses`.
