[CmdletBinding()]
param([string]$FfmpegPath = 'ffmpeg')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskFixtures = Join-Path $taskRoot 'build\fixtures'
[IO.Directory]::CreateDirectory($taskFixtures) | Out-Null
foreach ($taskCodec in @('h264', 'hevc')) {
    $taskOutput = Join-Path $taskFixtures "key-pattern.$taskCodec"
    $taskArguments = @('-hide_banner', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i',
        'color=c=0x00ff00:s=2560x1440:r=240', '-vf',
        'scale=in_color_matrix=bt601:out_color_matrix=bt709,drawbox=x=64:y=64:w=128:h=128:color=white:t=fill',
        '-frames:v', '12', '-pix_fmt', 'yuv420p', '-c:v', "${taskCodec}_nvenc",
        '-preset', 'p1', '-tune', 'ull', '-g', '1', '-bf', '0', '-rc', 'constqp', '-qp', '18',
        '-colorspace', 'bt709', '-color_trc', 'bt709', '-color_primaries', 'bt709', '-color_range', 'tv',
        '-f', $taskCodec, $taskOutput)
    & $FfmpegPath @taskArguments
    if ($LASTEXITCODE -ne 0) { throw "Fixture encoding failed for $taskCodec with code $LASTEXITCODE." }
    Get-FileHash -LiteralPath $taskOutput -Algorithm SHA256 | Select-Object Path,Hash
    # Inter-coded frames exercise the decoder while a separate worker presents.
    $taskInterOutput = Join-Path $taskFixtures "inter-pattern.$taskCodec"
    $taskInterArguments = @($taskArguments)
    $taskInterArguments[[Array]::IndexOf($taskInterArguments, '-frames:v') + 1] = '120'
    $taskInterArguments[[Array]::IndexOf($taskInterArguments, '-g') + 1] = '60'
    $taskInterArguments[-1] = $taskInterOutput
    & $FfmpegPath @taskInterArguments
    if ($LASTEXITCODE -ne 0) { throw "Inter-frame fixture encoding failed for $taskCodec with code $LASTEXITCODE." }
    Get-FileHash -LiteralPath $taskInterOutput -Algorithm SHA256 | Select-Object Path,Hash
    # A changing HUD workload for paced benchmarks, retaining the alpha-check anchors.
    $taskMovingOutput = Join-Path $taskFixtures "moving-pattern.$taskCodec"
    $taskMovingArguments = @('-hide_banner', '-loglevel', 'error', '-y',
        '-f', 'lavfi', '-i', 'color=c=0x00ff00:s=2560x1440:r=240',
        '-f', 'lavfi', '-i', 'color=c=white:s=64x64:r=240', '-filter_complex',
        "[0:v]drawbox=x=64:y=64:w=128:h=128:color=white:t=fill[base];[base][1:v]overlay=x='64+mod(n*17,2200)':y=700,scale=in_color_matrix=bt601:out_color_matrix=bt709",
        '-frames:v', '120', '-pix_fmt', 'yuv420p', '-c:v', "${taskCodec}_nvenc",
        '-preset', 'p1', '-tune', 'ull', '-g', '60', '-bf', '0', '-rc', 'constqp', '-qp', '18',
        '-colorspace', 'bt709', '-color_trc', 'bt709', '-color_primaries', 'bt709', '-color_range', 'tv',
        '-f', $taskCodec, $taskMovingOutput)
    & $FfmpegPath @taskMovingArguments
    if ($LASTEXITCODE -ne 0) { throw "Moving HUD fixture encoding failed for $taskCodec with code $LASTEXITCODE." }
    Get-FileHash -LiteralPath $taskMovingOutput -Algorithm SHA256 | Select-Object Path,Hash
}
