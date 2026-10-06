[CmdletBinding()]
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskDependencies = Join-Path $taskRoot 'native\dependencies'
$taskLock = Get-Content -LiteralPath (Join-Path $taskDependencies 'source-lock.json') -Raw | ConvertFrom-Json
$taskMoonlight = Join-Path $taskRoot 'build\references\moonlight-common-c'

function Invoke-StreamingTool {
    param([string]$Executable, [string[]]$ToolArguments)
    & $Executable @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited with code $LASTEXITCODE." }
}

& (Join-Path $PSScriptRoot 'BuildDependencies.ps1')
if (-not (Test-Path -LiteralPath (Join-Path $taskMoonlight '.git'))) {
    Invoke-StreamingTool 'git' @('clone', '--filter=blob:none', '--no-checkout', $taskLock.'moonlight-common-c'.repository, $taskMoonlight)
}
$taskChanges = & git -C $taskMoonlight status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $taskChanges) { throw 'Inspect changes in the Moonlight reference checkout before proceeding.' }
$taskRevision = & git -C $taskMoonlight rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $taskRevision -ne $taskLock.'moonlight-common-c'.revision) {
    Invoke-StreamingTool 'git' @('-C', $taskMoonlight, 'checkout', '--detach', $taskLock.'moonlight-common-c'.revision)
}
Invoke-StreamingTool 'git' @('-C', $taskMoonlight, 'submodule', 'update', '--init', '--recursive')
$taskSubmodules = & git -C $taskMoonlight submodule status --recursive
if ($LASTEXITCODE -ne 0 -or ($taskSubmodules | Where-Object { $_ -match '^[+\-U]' })) {
    throw 'Moonlight submodule revisions do not match the pinned parent.'
}
$taskToolchain = Join-Path $taskRoot 'build\tools\vcpkg\scripts\buildsystems\vcpkg.cmake'
$taskInstalled = Join-Path $taskRoot 'build\dependencies'
$taskBuild = Join-Path $taskRoot 'build\streaming'
Invoke-StreamingTool 'cmake' @('-S', (Join-Path $taskRoot 'native\streaming'), '-B', $taskBuild,
    '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_SYSTEM_NAME=WindowsStore',
    '-DCMAKE_SYSTEM_VERSION=10.0.26100.0', "-DCMAKE_TOOLCHAIN_FILE=$taskToolchain",
    '-DVCPKG_TARGET_TRIPLET=x64-uwp', '-DVCPKG_MANIFEST_MODE=OFF', "-DVCPKG_INSTALLED_DIR=$taskInstalled")
Invoke-StreamingTool 'cmake' @('--build', $taskBuild, '--config', $Configuration)
