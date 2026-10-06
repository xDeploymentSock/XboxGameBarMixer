[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskNativeRoot = $taskRoot
$taskManifest = Join-Path $taskNativeRoot 'native\dependencies'
$taskLock = Get-Content -LiteralPath (Join-Path $taskManifest 'source-lock.json') -Raw | ConvertFrom-Json
$taskVcpkgRoot = Join-Path $taskNativeRoot 'build\tools\vcpkg'
$taskInstallRoot = Join-Path $taskNativeRoot 'build\dependencies'
# FFmpeg's configure script rejects spaces in its source directory. Keep only
# the temporary build trees in a task-specific, space-free temporary path.
$taskBuildTrees = Join-Path ([IO.Path]::GetTempPath()) 'SoftwareFuserNative\buildtrees'
if ($taskBuildTrees.Contains(' ')) { throw 'FFmpeg needs a temporary build-tree path without spaces.' }

function Invoke-DependencyTool {
    param([string]$Executable, [string[]]$ToolArguments)
    & $Executable @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited with code $LASTEXITCODE." }
}

if (-not (Test-Path -LiteralPath (Join-Path $taskVcpkgRoot '.git'))) {
    Invoke-DependencyTool 'git' @('clone', '--filter=blob:none', '--no-checkout', $taskLock.vcpkg.repository, $taskVcpkgRoot)
}
$taskRevision = & git -C $taskVcpkgRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $taskRevision -ne $taskLock.vcpkg.revision) {
    Invoke-DependencyTool 'git' @('-C', $taskVcpkgRoot, 'checkout', '--detach', $taskLock.vcpkg.revision)
}
$taskActualRevision = & git -C $taskVcpkgRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $taskActualRevision -ne $taskLock.vcpkg.revision) { throw 'vcpkg revision verification failed.' }
$taskChanges = & git -C $taskVcpkgRoot status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $taskChanges) { throw 'The pinned vcpkg checkout has local changes; inspect them before building dependencies.' }

$taskVcpkg = Join-Path $taskVcpkgRoot 'vcpkg.exe'
if (-not (Test-Path -LiteralPath $taskVcpkg)) {
    Invoke-DependencyTool (Join-Path $taskVcpkgRoot 'bootstrap-vcpkg.bat') @('-disableMetrics')
}
[IO.Directory]::CreateDirectory($taskBuildTrees) | Out-Null
$taskPreviousMetrics = $env:VCPKG_DISABLE_METRICS
$taskPreviousConcurrency = $env:VCPKG_MAX_CONCURRENCY
try {
    $env:VCPKG_DISABLE_METRICS = '1'
    $env:VCPKG_MAX_CONCURRENCY = '8'
    Invoke-DependencyTool $taskVcpkg @('install', '--triplet=x64-uwp', "--x-manifest-root=$taskManifest", "--x-install-root=$taskInstallRoot", "--x-buildtrees-root=$taskBuildTrees", "--x-packages-root=$(Join-Path $taskNativeRoot 'build\vcpkg-packages')", "--overlay-ports=$(Join-Path $taskManifest 'ports')")
} finally {
    $env:VCPKG_DISABLE_METRICS = $taskPreviousMetrics
    $env:VCPKG_MAX_CONCURRENCY = $taskPreviousConcurrency
}
