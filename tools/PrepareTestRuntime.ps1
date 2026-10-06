[CmdletBinding()]
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskDestination = Join-Path $taskRoot "build\test-runtime\$Configuration"
$taskSdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx'
$taskPackage = if ($Configuration -eq 'Debug') {
    Join-Path $taskSdkRoot 'Debug\x64\Microsoft.VCLibs.x64.Debug.14.00.appx'
} else {
    Join-Path $taskSdkRoot 'Retail\x64\Microsoft.VCLibs.x64.14.00.appx'
}
if (-not (Test-Path -LiteralPath $taskPackage)) { throw "UWP test runtime is missing: $taskPackage" }
[IO.Directory]::CreateDirectory($taskDestination) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskPackage)
try {
    $taskCount = 0
    foreach ($taskEntry in $taskArchive.Entries) {
        if ($taskEntry.FullName -match '^[^/\\]+\.dll$') {
            $taskPath = Join-Path $taskDestination $taskEntry.Name
            [IO.Compression.ZipFileExtensions]::ExtractToFile($taskEntry, $taskPath, $true)
            $taskCount++
        }
    }
    if ($taskCount -eq 0) { throw 'The SDK runtime package contains no root DLLs.' }
    Write-Output "$taskCount SDK UWP runtime DLLs prepared for $Configuration tests. No package was installed."
} finally {
    $taskArchive.Dispose()
}
