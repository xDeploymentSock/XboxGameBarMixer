[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$HostAddress,
    [ValidateRange(1029, 65514)][int]$Port = 47989,
    [ValidateRange(1, 30)][int]$TimeoutSeconds = 8
)
$ErrorActionPreference = 'Stop'
$taskUri = [UriBuilder]::new('http', $HostAddress, $Port, '/serverinfo')
$taskUri.Query = 'uniqueid=SoftwareFuser'
$taskResponse = Invoke-WebRequest -Uri $taskUri.Uri -TimeoutSec $TimeoutSeconds -UseBasicParsing
$taskText = if ($taskResponse.Content -is [byte[]]) {
    [Text.Encoding]::UTF8.GetString($taskResponse.Content)
} else { [string]$taskResponse.Content }

$taskXmlSettings = [Xml.XmlReaderSettings]::new()
$taskXmlSettings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
$taskXmlSettings.XmlResolver = $null
$taskTextReader = [IO.StringReader]::new($taskText)
$taskReader = [Xml.XmlReader]::Create($taskTextReader, $taskXmlSettings)
try {
    $taskXml = [Xml.XmlDocument]::new()
    $taskXml.XmlResolver = $null
    $taskXml.Load($taskReader)
} finally {
    $taskReader.Dispose()
    $taskTextReader.Dispose()
}
if ($taskXml.DocumentElement.Name -ne 'root') { throw 'Unexpected Sunshine server-info document.' }
$taskFields = [ordered]@{ observedUtc = [DateTime]::UtcNow.ToString('o'); endpoint = $taskUri.Uri.GetLeftPart([UriPartial]::Path) }
foreach ($taskField in @('hostname', 'appversion', 'GfeVersion', 'HttpsPort', 'ExternalPort', 'MaxLumaPixelsHEVC', 'ServerCodecModeSupport', 'PairStatus', 'currentgame', 'state', 'gputype')) {
    $taskNode = $taskXml.DocumentElement.SelectSingleNode($taskField)
    if ($taskNode) { $taskFields[$taskField] = $taskNode.InnerText }
}
# Read-only: this command does not pair, launch, resume, quit, or unpair a host.
[pscustomobject]$taskFields | ConvertTo-Json -Depth 3
