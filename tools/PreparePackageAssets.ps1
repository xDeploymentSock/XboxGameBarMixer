# Creates placeholder package logos during a future explicit widget build.
# No downloads, installations, application launches, or display changes.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'

$taskRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskAssetDirectory = [System.IO.Path]::GetFullPath((Join-Path $taskRoot 'native\widget\Assets'))
if (-not $taskAssetDirectory.StartsWith($taskRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package assets must remain inside the repository.'
}
[System.IO.Directory]::CreateDirectory($taskAssetDirectory) | Out-Null
Add-Type -AssemblyName System.Drawing

$taskAssets = @(
    @{ Name = 'StoreLogo.png'; Width = 50; Height = 50 },
    @{ Name = 'Square44x44Logo.png'; Width = 44; Height = 44 },
    @{ Name = 'Square150x150Logo.png'; Width = 150; Height = 150 },
    @{ Name = 'SplashScreen.png'; Width = 620; Height = 300 }
)
foreach ($taskAsset in $taskAssets) {
    $taskBitmap = New-Object System.Drawing.Bitmap($taskAsset.Width, $taskAsset.Height)
    $taskGraphics = [System.Drawing.Graphics]::FromImage($taskBitmap)
    $taskBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 46, 216, 175))
    try {
        $taskGraphics.Clear([System.Drawing.Color]::Transparent)
        $taskSize = [Math]::Min($taskAsset.Width, $taskAsset.Height)
        $taskThickness = [Math]::Max(3, [int]($taskSize / 12))
        $taskX = [int](($taskAsset.Width - $taskSize) / 2 + $taskSize / 4)
        $taskY = [int](($taskAsset.Height - $taskSize) / 2 + $taskSize / 4)
        $taskGraphics.FillRectangle($taskBrush, $taskX, $taskY, $taskThickness, [int]($taskSize / 2))
        $taskGraphics.FillRectangle($taskBrush, $taskX, $taskY, [int]($taskSize / 2), $taskThickness)
        $taskGraphics.FillRectangle($taskBrush, $taskX, $taskY + [int]($taskSize / 5),
                                    [int]($taskSize / 3), $taskThickness)
        $taskBitmap.Save((Join-Path $taskAssetDirectory $taskAsset.Name), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $taskBrush.Dispose()
        $taskGraphics.Dispose()
        $taskBitmap.Dispose()
    }
}
