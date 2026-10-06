# Native streaming dependencies

vcpkg.json and source-lock.json pin the dependency and source baselines.
Run tools/BuildDependencies.ps1 explicitly to build x64 UWP libraries.
The current widget does not link them yet.

The FFmpeg overlay is copied from microsoft/vcpkg revision
9e593bb18ea69cc5095e012465dcd675a822ed0d, port 8.1.2#3.
The vcpkg MIT license is included in ports/VCPKG-LICENSE.txt.
The only initial port change passes MSVC library search paths through response
files, preserving spaces in the workspace path. The original build failed
with LNK1181 on Fuser\build\dependencies\x64-uwp\lib.obj, because FFmpeg's
configure script split the unquoted library path.

Build trees use a task-specific temporary directory without spaces because
FFmpeg itself rejects spaces in its source path. The installed libraries
and source lock stay in the workspace. The script verifies the pinned
vcpkg checkout, disables metrics for the process, and limits compilation
to eight jobs.

FFmpeg has only the avcodec feature enabled; GPL and nonfree features are
not requested. Dependency copyright files are installed under
build/dependencies/x64-uwp/share/<package>/copyright.
