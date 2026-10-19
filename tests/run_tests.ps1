# Text2yks の単体テスト（Text2yksCli で DLL を呼び、結果を期待値と比べる）
#
# tests\cases\<名前>\ の中身:
#   Text2yks.txt（＋読み込むファイル）  定義
#   *.yks                               入力（名前順に処理する）
#   *.expected                          *.yks の期待する出力（先頭の UTF-8 BOM は比べる前に除く）
#   expected_log.txt                    （任意）各行がログ（標準エラー）に含まれること
#   expected_stdout.txt                 （任意）各行が標準出力に含まれること
#   cli_args.txt                        （任意）Text2yksCli に足す引数（1行に1つ）
#   use_definition.txt                  （任意）リポジトリからの相対パス。そのファイルを Text2yks.txt として使う（examples の確認用）
# ケースのフォルダは作業用に build\tests\<x86|x64>\<構成>\ へ写してから使う（定義ファイルを書き換えるテストがあるため）
#
# 例: powershell -File tests\run_tests.ps1 -Platform x64 -Config Release
#     powershell -File tests\run_tests.ps1 -Toolset vs2008 -Config Debug     （VS2008 のビルド。x86 のみ）
# 終了コード: 失敗したケースの数
param(
    [ValidateSet('x86', 'x64')]
    [string]$Platform = 'x64',
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    [ValidateSet('vs2026', 'vs2008')]
    [string]$Toolset = 'vs2026',    # vs2008 は build\vs2008\ の下を使う
    [string]$Case = ''              # 指定すればそのケースだけ
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $root 'build'
if ($Toolset -eq 'vs2008') { $Platform = 'x86'; $buildRoot = Join-Path $buildRoot 'vs2008' }
$cli  = Join-Path $buildRoot "Text2yksCli\$Platform\$Config\Text2yksCli_$Platform.exe"
$dll  = Join-Path $buildRoot "Text2yks\$Platform\$Config\Text2yks_$Platform.dll"
foreach ($f in @($cli, $dll)) {
    if (-not (Test-Path $f)) { Write-Host "見つかりません: $f"; exit 100 }
}
$work = Join-Path $root "build\tests\$Toolset\$Platform\$Config"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory $work | Out-Null

# CLI の出力は UTF-8
$prevEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

function Remove-Bom([byte[]]$bytes) {
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        if ($bytes.Length -eq 3) { return ,([byte[]]@()) }
        return ,([byte[]]$bytes[3..($bytes.Length - 1)])
    }
    return ,$bytes
}
function Read-Lines([string]$path) {
    $text = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
    return @($text -split "`r?`n" | Where-Object { $_ -ne '' })
}

$failed = 0
$cases = Get-ChildItem (Join-Path $PSScriptRoot 'cases') -Directory | Sort-Object Name
if ($Case -ne '') { $cases = @($cases | Where-Object { $_.Name -eq $Case }) }
foreach ($c in $cases) {
    $dir = Join-Path $work $c.Name
    Copy-Item $c.FullName $dir -Recurse
    $useFile = Join-Path $dir 'use_definition.txt'
    if (Test-Path $useFile) { Copy-Item (Join-Path $root @(Read-Lines $useFile)[0]) (Join-Path $dir 'Text2yks.txt') -Force }
    $outdir = Join-Path $dir '_out'
    $inputs = @(Get-ChildItem $dir -Filter '*.yks' -File | Sort-Object Name)
    $cliArgs = @('--dll', $dll, '--data', $dir, '--out', $outdir)
    $argsFile = Join-Path $dir 'cli_args.txt'
    if (Test-Path $argsFile) { $cliArgs += Read-Lines $argsFile }
    $cliArgs += $inputs | ForEach-Object { $_.FullName }

    $stderrFile = Join-Path $dir '_stderr.txt'
    $stdout = & $cli @cliArgs 2> $stderrFile
    $exit = $LASTEXITCODE
    $stderr = if (Test-Path $stderrFile) { [IO.File]::ReadAllText($stderrFile, [Text.Encoding]::UTF8) } else { '' }
    $stdoutText = ($stdout | Out-String)

    $problems = @()
    if ($exit -ne 0) { $problems += "Text2yksCli の終了コードが $exit" }
    foreach ($in in $inputs) {
        $expectedPath = [IO.Path]::ChangeExtension($in.FullName, '.expected')
        $actualPath = Join-Path $outdir $in.Name
        if (-not (Test-Path $expectedPath)) { $problems += "$($in.Name): 期待値ファイルがありません"; continue }
        if (-not (Test-Path $actualPath))   { $problems += "$($in.Name): 出力がありません"; continue }
        $expected = Remove-Bom ([IO.File]::ReadAllBytes($expectedPath))
        $actual   = [IO.File]::ReadAllBytes($actualPath)
        if ([Convert]::ToBase64String($expected) -ne [Convert]::ToBase64String($actual)) {
            $problems += "$($in.Name): 出力が期待値と違います"
            $problems += "  期待: " + ([Text.Encoding]::UTF8.GetString($expected) -replace "`r", '\r' -replace "`n", '\n' -replace "`t", '\t')
            $problems += "  実際: " + ([Text.Encoding]::UTF8.GetString($actual)   -replace "`r", '\r' -replace "`n", '\n' -replace "`t", '\t')
        }
    }
    $logFile = Join-Path $dir 'expected_log.txt'
    if (Test-Path $logFile) {
        foreach ($line in Read-Lines $logFile) {
            if (-not $stderr.Contains($line)) { $problems += "ログにありません: $line" }
        }
    }
    $outFile = Join-Path $dir 'expected_stdout.txt'
    if (Test-Path $outFile) {
        foreach ($line in Read-Lines $outFile) {
            if (-not $stdoutText.Contains($line)) { $problems += "標準出力にありません: $line" }
        }
    }

    if ($problems.Count -eq 0) {
        Write-Host "PASS  $($c.Name)"
    } else {
        $failed++
        Write-Host "FAIL  $($c.Name)"
        $problems | ForEach-Object { Write-Host "      $_" }
        if ($stderr.Trim() -ne '') { Write-Host "      --- ログ ---"; ($stderr.TrimEnd() -split "`r?`n") | ForEach-Object { Write-Host "      $_" } }
    }
}
[Console]::OutputEncoding = $prevEncoding
Write-Host ("{0}  ({1} 件中 {2} 件失敗)  {3} {4}|{5}" -f $(if ($failed -eq 0) { 'ALL PASSED' } else { 'FAILED' }), $cases.Count, $failed, $Toolset, $Config, $Platform)
exit $failed
