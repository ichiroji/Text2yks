# DLL と一緒に配るライセンス表記 Text2yks_LICENSE.txt を作る
#
# Text2yks の LICENSE（MIT）と、同梱している PCRE2 の LICENCE の全文をまとめる。
# PCRE2 を含む DLL を配るときは、この表記を一緒に配る（PCRE2 の BSD ライセンスの条件）。
# PCRE2 の版を上げたときは作り直す。
#
# 例: powershell -File tools\make_license_notice.ps1 -Out 'D:\Game\Text2yks_LICENSE.txt'
param(
    [Parameter(Mandatory = $true)]
    [string]$Out
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$pcreDir = @(Get-ChildItem (Join-Path $root 'third_party\pcre2') -Directory | Where-Object { $_.Name -match '^pcre2-\d' })
if ($pcreDir.Count -ne 1) { throw "third_party\pcre2 に pcre2-<版> のフォルダが1つだけあること（見つかった数: $($pcreDir.Count)）" }
$version = $pcreDir[0].Name.Substring(6)
$mit  = [IO.File]::ReadAllText((Join-Path $root 'LICENSE'))
$pcre = [IO.File]::ReadAllText((Join-Path $pcreDir[0].FullName 'LICENCE'))

$text = @"
Text2yks_x86.dll / Text2yks_x64.dll  -  Yuka2 script preprocessor
====================================================================

This software (Text2yks) is distributed under the MIT License.
It contains PCRE2 $version (https://github.com/PCRE2Project/pcre2), which is distributed
under the BSD licence shown below.

--------------------------------------------------------------------
Text2yks
--------------------------------------------------------------------

$mit
--------------------------------------------------------------------
PCRE2
--------------------------------------------------------------------

$pcre
"@
$text = ($text -replace "`r`n", "`n") -replace "`n", "`r`n"
[IO.File]::WriteAllText($Out, $text, (New-Object System.Text.UTF8Encoding($true)))
Write-Host "書きました: $Out（PCRE2 $version）"
