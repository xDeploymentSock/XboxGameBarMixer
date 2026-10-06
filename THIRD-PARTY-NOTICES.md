# Third-party notices

The Game Bar activation approach, NuGet baseline, and metadata marshaling entries are adapted from Microsoft's XboxGameBarSamples. Its license is reproduced below. The new renderer and core do not include copied Moonlight, Sunshine, FFmpeg, or the game-specific PoC source.

The streaming widget links Moonlight's GPL-3.0 core and bundles the dependency licenses. Distribution of the combined application must meet GPL-3.0's corresponding-source and license obligations; this local development installation is not a distribution release. FFmpeg licensing depends on its build options. No independent permissive redistribution license has been selected for the combined application.

## Streaming dependencies

The FFmpeg vcpkg overlay is copied from the pinned Microsoft vcpkg port and contains a local library-path response-file fix. The vcpkg MIT license is reproduced in native/dependencies/ports/VCPKG-LICENSE.txt; FFmpeg's own license remains separate.

`native/dependencies/source-lock.json` pins vcpkg, the official Moonlight streaming core, and the Moonlight Xbox reference. The widget uses FFmpeg 8.1.2 (avcodec without GPL/nonfree extras), curl 8.21.0, OpenSSL 3.6.3, Expat 2.8.2, and zlib 1.3.2 built for x64 UWP. Their installed copyright files are copied into `third_party/licenses` and packaged under `Licenses`. Successful builds and fixtures are distinct from verified live streaming.

The official Moonlight core revision `f900dd4767759c7b9d0e93bcea666b55c69ea62f` is GPL-3.0; its license is copied to `third_party/licenses/moonlight-common-c.txt`. Its pinned ENet and nanors submodules have MIT licenses copied beside it. The parent source checkout retains additional source-file notices. CMake builds an isolated source copy and removes the two mouse-wake calls and sleeps in `Connection.c`; it fails if the pinned patch context changes. Keep that modification, all app source, build scripts, dependency overlays, and the exact dependency sources with the corresponding source when distributing a combined package.

The new FFmpeg adapter uses the documented D3D11VA API; no Moonlight Xbox decoder source has been copied into the application. The existing user-installed FFmpeg command-line tool generates owned green/white test fixtures only and is not bundled in the widget.

## XboxGameBarSamples

MIT License

Copyright (c) Microsoft Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
