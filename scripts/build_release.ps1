param([string] $Upstream, [switch] $Fast)
. "$PSScriptRoot/common.ps1"
Build-Chibi Release $Upstream $Fast.IsPresent
