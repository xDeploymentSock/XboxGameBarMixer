[CmdletBinding()]
param([ValidateRange(6, 60)][int]$WaitSeconds = 30)
$ErrorActionPreference = 'Stop'
$probeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$probePackage = Get-AppxPackage -Name SoftwareFuser.Widget
if (-not $probePackage -or $probePackage.Version -lt [version]'0.2.0.12') {
    throw 'Install the 0.2.0.12 URI-enabled probe or a later version first.'
}
[xml]$probeManifest = Get-Content -LiteralPath (Join-Path $probePackage.InstallLocation 'AppxManifest.xml') -Raw
$probeApplication = [string]$probeManifest.Package.Applications.Application.Id
$probeExtension = [string]$probeManifest.Package.Applications.Application.Extensions.Extension.AppExtension.Id
if ($probeExtension -ne 'RemoteHudStartupTest') { throw 'Expected the coverage-test widget extension.' }
$probeWidgetId = $probePackage.PackageFamilyName + '_' + $probeApplication + '_' + $probeExtension
$probeUri = 'ms-gamebar://launch/activate/' + $probeWidgetId + '?coverage=pinned'
$probeLog = Join-Path $env:LOCALAPPDATA ('Packages/' + $probePackage.PackageFamilyName + '/LocalState/runtime.log')
$probeCutoff = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$probeClock = [Diagnostics.Stopwatch]::StartNew()
Start-Process -FilePath $probeUri
Write-Output 'Requested the one-shot pinned test through the documented Game Bar activation URI.'
while ($probeClock.Elapsed.TotalSeconds -lt $WaitSeconds) {
    $probeLines = @(Get-Content -LiteralPath $probeLog -Tail 250 | Where-Object {
        $_ -match '^(\d+) ' -and [long]$Matches[1] -ge $probeCutoff
    })
    $probeSettled = $probeLines | Select-String -Pattern 'Pinned constraints settled after five seconds: accepted=(true|false) geometryAligned=(true|false) pinned=(true|false) mode=(\d+)' | Select-Object -Last 1
    if ($probeSettled) {
        $probeEvidence = @($probeLines | Where-Object {
            $_ -match 'App created|Widget launch|Widget repeat|Fixed startup|URI pinned probe|Pinned constraints|pinned constraints|View geometry|Widget state|Edge gaps|coverage test|test bounds'
        })
        $probeResult = [ordered]@{
            Version = $probePackage.Version.ToString()
            ObservedProbe = $true
            HostAccepted = $probeSettled.Matches[0].Groups[1].Value -eq 'true'
            GeometryAligned = $probeSettled.Matches[0].Groups[2].Value -eq 'true'
            Pinned = $probeSettled.Matches[0].Groups[3].Value -eq 'true'
            DisplayMode = [int]$probeSettled.Matches[0].Groups[4].Value
            VisualCoverageVerified = $false
            Evidence = $probeEvidence
        }
        $probeResult | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $probeRoot 'build/pinned-uri-probe-result.json') -Encoding utf8
        $probeResult | ConvertTo-Json -Depth 4
        exit 0 # Observation completed, not proof of visible coverage.
    }
    $probeRefused = $probeLines | Select-String -Pattern 'URI pinned probe requires|URI pinned probe could not' | Select-Object -Last 1
    if ($probeRefused) { Write-Output $probeRefused.Line; exit 2 }
    Start-Sleep -Milliseconds 500
}
Write-Output 'No completed pinned test observation. Check activation, pin state, and local runtime evidence.'
exit 3
