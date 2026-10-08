# Installs this workspace's unsigned development package using Windows 11's
# per-package AllowUnsigned support. Does not enable global Developer Mode.
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$Elevated,
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskSuffix = if ($Configuration -eq 'Debug') { '_Debug' } else { '' }
$taskVersion = ([xml](Get-Content -LiteralPath (Join-Path $taskRoot 'native\widget\Package.appxmanifest') -Raw)).Package.Identity.Version
if ($taskVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Invalid project package version.' }
$taskPackage = Join-Path $taskRoot "AppPackages\FuserWidget\FuserWidget_$($taskVersion)_x64$($taskSuffix)_Test\FuserWidget_$($taskVersion)_x64$($taskSuffix).msix"
if (-not (Test-Path -LiteralPath $taskPackage)) { throw 'Build this configuration before deploying.' }
$taskUnsignedPublisher = 'CN=SoftwareFuser.Development, OID.2.25.311729368913984317654407730594956997722=1'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskPackage)
try {
    $taskManifestEntry = $taskArchive.GetEntry('AppxManifest.xml')
    if (-not $taskManifestEntry) { throw 'Package has no manifest.' }
    $taskStream = $taskManifestEntry.Open()
    try {
        $taskManifest = [Xml.XmlDocument]::new()
        $taskManifest.XmlResolver = $null
        $taskManifest.Load($taskStream)
    } finally { $taskStream.Dispose() }
    if ($taskManifest.Package.Identity.Name -ne 'SoftwareFuser.Widget' -or
        $taskManifest.Package.Identity.Publisher -ne $taskUnsignedPublisher -or
        $taskManifest.Package.Identity.Version -ne $taskVersion -or
        $taskManifest.Package.Identity.ProcessorArchitecture -ne 'x64') {
        throw 'Package identity, version and architecture must match this x64 development checkout.'
    }
} finally { $taskArchive.Dispose() }

if ($ValidateOnly) {
    $taskHasher = [Security.Cryptography.SHA256]::Create()
    $taskPackageStream = [IO.File]::OpenRead($taskPackage)
    try {
        $taskPackageHash = [BitConverter]::ToString($taskHasher.ComputeHash($taskPackageStream)).Replace('-', '').ToLowerInvariant()
    } finally { $taskPackageStream.Dispose(); $taskHasher.Dispose() }
    [pscustomobject]@{
        Name = $taskManifest.Package.Identity.Name
        Version = $taskVersion
        Architecture = 'x64'
        PackageSha256 = $taskPackageHash
        ValidatedOnly = $true
    } | ConvertTo-Json -Compress
    return
}

$taskIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$taskPrincipal = [Security.Principal.WindowsPrincipal]::new($taskIdentity)
if (-not $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Windows administrator elevation was not granted.' }
    $taskArguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Configuration', $Configuration, '-Elevated')
    $taskProcess = Start-Process -FilePath 'powershell.exe' -ArgumentList $taskArguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    if ($taskProcess.ExitCode -ne 0) { throw "Package installer exited with code $($taskProcess.ExitCode)." }
} else {
    $taskDependencies = @()
    if ($Configuration -eq 'Debug') {
        $taskRuntime = Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Debug\x64\Microsoft.VCLibs.x64.Debug.14.00.appx'
        if (-not (Test-Path -LiteralPath $taskRuntime)) { throw 'UWP debug runtime is missing; use Release or install the UWP build tools.' }
        $taskDependencies += $taskRuntime
    }
    if ($taskDependencies.Count -gt 0) {
        Add-AppxPackage -Path $taskPackage -AllowUnsigned -DependencyPath $taskDependencies -ForceApplicationShutdown
    } else {
        Add-AppxPackage -Path $taskPackage -AllowUnsigned -ForceApplicationShutdown
    }
}
$taskInstalled = Get-AppxPackage -Name 'SoftwareFuser.Widget'
if (-not $taskInstalled -or $taskInstalled.Version -ne [version]$taskVersion -or
    $taskInstalled.Publisher -ne $taskUnsignedPublisher -or $taskInstalled.Architecture -ne 'X64' -or
    $taskInstalled.Status -ne 'Ok') { throw 'Installed package identity, version, architecture or status did not match the requested development build.' }
$taskInstalled | Select-Object Name, PackageFamilyName, Version, InstallLocation, Status | ConvertTo-Json -Compress
