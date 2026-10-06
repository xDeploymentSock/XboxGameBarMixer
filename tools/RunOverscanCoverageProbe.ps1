[CmdletBinding()]
param([ValidateRange(6, 60)][int]$WaitSeconds = 30)
$ErrorActionPreference = 'Stop'
$probeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$probePackage = Get-AppxPackage -Name SoftwareFuser.Widget
if (-not $probePackage -or $probePackage.Version -lt [version]'0.2.0.14') {
    throw 'Install the 0.2.0.14 overscan viewport test or a later version first.'
}
[xml]$probeManifest = Get-Content -LiteralPath (Join-Path $probePackage.InstallLocation 'AppxManifest.xml') -Raw
$probeApplication = [string]$probeManifest.Package.Applications.Application.Id
$probeExtension = [string]$probeManifest.Package.Applications.Application.Extensions.Extension.AppExtension.Id
if ($probeExtension -ne 'RemoteHudOverscanTest') { throw 'Expected the overscan-test widget extension.' }
$probeWidgetId = $probePackage.PackageFamilyName + '_' + $probeApplication + '_' + $probeExtension
$probeLog = Join-Path $env:LOCALAPPDATA ('Packages/' + $probePackage.PackageFamilyName + '/LocalState/runtime.log')
$probeCutoff = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$probeClock = [Diagnostics.Stopwatch]::StartNew()
Start-Process -FilePath ('ms-gamebar://launch/activate/' + $probeWidgetId + '?coverage=startup')
Write-Output 'Requested the overscan diagnostic through the public Game Bar activation URI. No resize is requested.'
while ($probeClock.Elapsed.TotalSeconds -lt $WaitSeconds) {
    $probeLines = @(Get-Content -LiteralPath $probeLog -Tail 250 | Where-Object {
        $_ -match '^(\d+) ' -and [long]$Matches[1] -ge $probeCutoff
    })
    $probeSettled = $probeLines | Select-String -Pattern 'Overscan viewport settled: geometryAligned=(true|false) pinned=(true|false) visible=(true|false) mode=(\d+)' | Select-Object -Last 1
    if ($probeSettled) {
        $probeResult = [ordered]@{
            Version = $probePackage.Version.ToString()
            ObservedProbe = $true
            GeometryAligned = $probeSettled.Matches[0].Groups[1].Value -eq 'true'
            Pinned = $probeSettled.Matches[0].Groups[2].Value -eq 'true'
            Visible = $probeSettled.Matches[0].Groups[3].Value -eq 'true'
            DisplayMode = [int]$probeSettled.Matches[0].Groups[4].Value
            VisualCoverageVerified = $false
            Evidence = @($probeLines | Where-Object {
                $_ -match 'App created|Widget launch|Widget repeat|Fixed startup|URI startup probe|Overscan|View geometry|Widget state|Video area|Draw requested|Preview key|One test frame'
            })
        }
        $probeResult | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $probeRoot 'build/overscan-uri-probe-result.json') -Encoding utf8
        $probeResult | ConvertTo-Json -Depth 4
        exit 0 # Completed observation, not proof of visible coverage or pinning.
    }
    $probeRefused = $probeLines | Select-String -Pattern 'URI startup probe requires' | Select-Object -Last 1
    if ($probeRefused) { Write-Output $probeRefused.Line; exit 2 }
    Start-Sleep -Milliseconds 500
}
Write-Output 'No completed overscan observation. Check activation and local runtime evidence.'
exit 3
