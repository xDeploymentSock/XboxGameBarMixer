[CmdletBinding()]
param(
    [ValidateSet('Core', 'GPU', 'Decoder', 'Control', 'Widget', 'All')][string]$Target = 'All',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVsWhere)) { throw 'Visual Studio 2022 and vswhere are required.' }
$taskVsRoot = & $taskVsWhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskVsRoot) { throw 'Visual Studio 2022 C++ x64 tools are missing.' }
$taskMsBuild = Join-Path $taskVsRoot 'MSBuild\Current\Bin\MSBuild.exe'

function Invoke-TaskTool {
    param([string]$Executable, [string[]]$ToolArguments)
    & $Executable @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited with code $LASTEXITCODE." }
}

Push-Location -LiteralPath $taskRoot
try {
    if ($Target -in @('Core', 'All')) {
        Invoke-TaskTool 'cmake' @('-S', '.', '-B', 'build/core', '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$taskVsRoot", '-DFUSER_BUILD_TESTS=ON')
        Invoke-TaskTool 'cmake' @('--build', 'build/core', '--config', $Configuration)
        Invoke-TaskTool 'ctest' @('--test-dir', 'build/core', '-C', $Configuration, '--output-on-failure')
    }
    if ($Target -in @('GPU', 'All')) {
        Invoke-TaskTool 'cmake' @('-S', '.', '-B', 'build/gpu', '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$taskVsRoot", '-DFUSER_BUILD_TESTS=ON', '-DFUSER_BUILD_GPU_TESTS=ON')
        Invoke-TaskTool 'cmake' @('--build', 'build/gpu', '--config', $Configuration)
        Invoke-TaskTool 'ctest' @('--test-dir', 'build/gpu', '-C', $Configuration, '--output-on-failure')
    }
    if ($Target -in @('Widget', 'All')) {
        & (Join-Path $PSScriptRoot 'BuildStreamingLibraries.ps1') -Configuration $Configuration
        $taskUwpRoot = & $taskVsWhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.ComponentGroup.UWP.VC -property installationPath
        if ($taskUwpRoot -ne $taskVsRoot) { throw 'Add C++ (v143) Universal Windows Platform tools to this Visual Studio instance.' }
        [IO.Directory]::CreateDirectory((Join-Path $taskRoot 'build')) | Out-Null
        $taskLog = Join-Path $taskRoot "build/widget-$Configuration.log"
        $taskProperties = @("/p:Configuration=$Configuration", '/p:Platform=x64', '/v:minimal', '/nologo')
        Invoke-TaskTool $taskMsBuild (@('SoftwareFuser.sln', '/t:Restore', '/p:RestorePackagesConfig=true') + $taskProperties)
        Invoke-TaskTool $taskMsBuild (@('SoftwareFuser.sln', '/t:Build', '/fl', "/flp:logfile=$taskLog;verbosity=normal") + $taskProperties)
    }
    if ($Target -eq 'Decoder') {
        & (Join-Path $PSScriptRoot 'BuildDependencies.ps1')
        & (Join-Path $PSScriptRoot 'PrepareTestRuntime.ps1') -Configuration $Configuration
        & (Join-Path $PSScriptRoot 'GenerateDecoderFixtures.ps1')
        Invoke-TaskTool 'cmake' @('-S', '.', '-B', 'build/decoder', '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$taskVsRoot", '-DFUSER_BUILD_TESTS=ON', '-DFUSER_BUILD_GPU_TESTS=ON', '-DFUSER_BUILD_DECODER_TESTS=ON')
        Invoke-TaskTool 'cmake' @('--build', 'build/decoder', '--config', $Configuration)
        Invoke-TaskTool 'ctest' @('--test-dir', 'build/decoder', '-C', $Configuration, '--output-on-failure')
    }
    if ($Target -eq 'Control') {
        & (Join-Path $PSScriptRoot 'BuildStreamingLibraries.ps1') -Configuration $Configuration
        & (Join-Path $PSScriptRoot 'PrepareTestRuntime.ps1') -Configuration $Configuration
        $taskRuntimePython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        $taskPython = if (Test-Path -LiteralPath $taskRuntimePython) { $taskRuntimePython } else { (Get-Command python -ErrorAction Stop).Source }
        Invoke-TaskTool $taskPython @('-c', 'import cryptography')
        Invoke-TaskTool 'cmake' @('-S', '.', '-B', 'build/control', '-G', 'Visual Studio 17 2022', '-A', 'x64',
            '-DFUSER_BUILD_CONTROL_TESTS=ON', "-DFUSER_TEST_PYTHON=$taskPython")
        Invoke-TaskTool 'cmake' @('--build', 'build/control', '--config', $Configuration)
        Invoke-TaskTool 'ctest' @('--test-dir', 'build/control', '-C', $Configuration, '--output-on-failure')
    }
} finally {
    Pop-Location
}
