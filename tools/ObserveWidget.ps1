[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$WidgetProcessId,
    [ValidateRange(1, 3600)][int]$DurationSeconds = 1800,
    [ValidateRange(1, 60)][int]$PollSeconds = 15
)
$ErrorActionPreference = 'Stop'
$observationRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$observationLog = Join-Path $env:LOCALAPPDATA 'Packages/SoftwareFuser.Widget_6g84c2f4w9w1a/LocalState/runtime.log'
$observationCsv = Join-Path $observationRoot ('build/widget-observation-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.csv')
[IO.Directory]::CreateDirectory((Split-Path -Parent $observationCsv)) | Out-Null
$observedWidget = Get-Process -Id $WidgetProcessId
try {
    if ($observedWidget.ProcessName -ne 'FuserWidget') { throw 'Select an existing Software Fuser widget process.' }
    $observedStart = $observedWidget.StartTime
} finally { $observedWidget.Dispose() }
$observationPattern = '(?m)^(\d+) (HEVC|H\.264|AV1) (\d+)x(\d+) / setup FPS (\d+)\r?\nReceived units/s ([\d.]+) \| Decoded/s ([\d.]+) \| Present calls/s ([\d.]+)\r?\nReplaced display frames (\d+) \| Decode errors (\d+)'
$observationClock = [Diagnostics.Stopwatch]::StartNew()
Write-Output "Observing widget PID $WidgetProcessId; CSV: $observationCsv"
do {
    $observedWidget = Get-Process -Id $WidgetProcessId -ErrorAction SilentlyContinue
    if (-not $observedWidget) { Write-Output 'Observed widget process exited.'; break }
    try {
        if ($observedWidget.ProcessName -ne 'FuserWidget' -or $observedWidget.StartTime -ne $observedStart) {
            Write-Output 'Observed process identity changed.'
            break
        }
        $observationText = [IO.File]::ReadAllText($observationLog)
        $observationMatches = [regex]::Matches($observationText, $observationPattern)
        $latestObservation = if ($observationMatches.Count) { $observationMatches[$observationMatches.Count - 1] } else { $null }
        $observationTime = [DateTimeOffset]::UtcNow
        $sampleTime = if ($latestObservation) { [DateTimeOffset]::FromUnixTimeMilliseconds([long]$latestObservation.Groups[1].Value) } else { $null }
        $observation = [pscustomobject][ordered]@{
            observed_utc = $observationTime.ToString('o')
            pid = $WidgetProcessId
            process_cpu_seconds = $observedWidget.TotalProcessorTime.TotalSeconds
            working_set_bytes = $observedWidget.WorkingSet64
            private_bytes = $observedWidget.PrivateMemorySize64
            handles = $observedWidget.HandleCount
            last_rate_sample_utc = if ($sampleTime) { $sampleTime.ToString('o') } else { $null }
            rate_sample_age_seconds = if ($sampleTime) { ($observationTime - $sampleTime).TotalSeconds } else { $null }
            codec = if ($latestObservation) { $latestObservation.Groups[2].Value } else { $null }
            width = if ($latestObservation) { $latestObservation.Groups[3].Value } else { $null }
            height = if ($latestObservation) { $latestObservation.Groups[4].Value } else { $null }
            setup_fps = if ($latestObservation) { $latestObservation.Groups[5].Value } else { $null }
            received_units_per_second = if ($latestObservation) { $latestObservation.Groups[6].Value } else { $null }
            decoded_per_second = if ($latestObservation) { $latestObservation.Groups[7].Value } else { $null }
            present_calls_per_second = if ($latestObservation) { $latestObservation.Groups[8].Value } else { $null }
            replaced_display_frames = if ($latestObservation) { $latestObservation.Groups[9].Value } else { $null }
            decode_errors = if ($latestObservation) { $latestObservation.Groups[10].Value } else { $null }
            disconnect_after_sample = if ($latestObservation) {
                $observationText.LastIndexOf(' Disconnected.') -gt $latestObservation.Index
            } else { $null }
        }
        $observation | Export-Csv -LiteralPath $observationCsv -NoTypeInformation -Append
        Write-Output ($observationTime.ToString('HH:mm:ss') + ' UTC; received/s=' + $observation.received_units_per_second +
            '; private MiB=' + [math]::Round($observation.private_bytes / 1MB, 1) +
            '; decode errors=' + $observation.decode_errors +
            '; sample age seconds=' + [math]::Round($observation.rate_sample_age_seconds, 1))
    } finally { $observedWidget.Dispose() }
    $remainingObservationSeconds = $DurationSeconds - $observationClock.Elapsed.TotalSeconds
    if ($remainingObservationSeconds -le 0) { break }
    Start-Sleep -Milliseconds ([int][math]::Min($PollSeconds * 1000, $remainingObservationSeconds * 1000))
} while ($observationClock.Elapsed.TotalSeconds -le $DurationSeconds + 1)
Write-Output "Observation finished; CSV: $observationCsv"
