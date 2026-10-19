# 旧 Text2yks（alias 定義）と新 Text2yks（Text2yks.txt）の出力を、ゲームの全 .yks で比べる
#
# Text2yksCli で同じ .yks 一式を次の3通りに通し、行ごとに比べる（行数が変わらないことも確かめる）
#   旧     旧 DLL（ゲームフォルダの Text2yks_<x86|x64>.dll）＋元の alias（今のゲームの動き）
#   旧BOM  旧 DLL＋alias に UTF-8 の BOM を付けた写し（旧版が BOM 無しを CP932 として読む不具合を除いたもの）
#   新     新 DLL（このリポジトリの Release ビルド）＋変換した Text2yks.txt
# 差の分類
#   文字コード   旧 ≠ 旧BOM（旧版が定義を CP932 として読んで化けていた）
#   末尾TAB      旧BOM ≠ 新 だが TAB を除くと同じ（旧の置き換え後の末尾の TAB は新しい形では書けない）
#   要確認       それ以外の 旧BOM ≠ 新（理由を1行ずつ確かめる）
#
# 例: powershell -File tools\compare_old_new.ps1 -Data 'D:\Game' -Definition 'D:\Game\Text2yks.txt' -Report 'D:\report.md'
# 終了コード: 0＝行数が全部同じ・1＝行数が変わったファイルがある・2＝実行の誤り
param(
    [Parameter(Mandatory = $true)]
    [string]$Data,                                  # ゲームフォルダ（旧 DLL と alias*.txt がある所）
    [Parameter(Mandatory = $true)]
    [string]$Definition,                            # 新しい Text2yks.txt
    [string]$Report = '',                           # 差分一覧（Markdown）の書き先。省略時は表示だけ
    [string[]]$Platform = @('x64', 'x86'),
    [string]$Config = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$Data = (Resolve-Path $Data).Path
$Definition = (Resolve-Path $Definition).Path
$game = Split-Path $Data -Leaf
$utf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = $utf8

function Read-Lines([string]$path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $start = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $start = 3 }
    $text = $utf8.GetString($bytes, $start, $bytes.Length - $start)
    return , ($text -split "`r`n|`n|`r")
}
function Show([string]$s) { return ($s -replace '\|', '\|' -replace "`t", '→') }

$aliasFiles = @(Get-ChildItem $Data -File | Where-Object { $_.Name -match '^alias[0-2]\.txt$' } | Sort-Object Name)
$summary = New-Object System.Collections.Generic.List[string]
$details = New-Object System.Collections.Generic.List[string]
$exitCode = 0
$newOutputs = @{}

foreach ($p in $Platform) {
    $cli    = Join-Path $root "build\Text2yksCli\$p\$Config\Text2yksCli_$p.exe"
    $newDll = Join-Path $root "build\Text2yks\$p\$Config\Text2yks_$p.dll"
    $oldDll = Join-Path $Data "Text2yks_$p.dll"
    foreach ($f in @($cli, $newDll, $oldDll)) { if (-not (Test-Path $f)) { Write-Host "見つかりません: $f"; exit 2 } }

    $work = Join-Path $root "build\compare\$game\$p"
    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
    $defs = @{ old = "$work\def_old"; oldbom = "$work\def_oldbom"; new = "$work\def_new" }
    foreach ($d in $defs.Values) { New-Item -ItemType Directory -Force $d | Out-Null }
    foreach ($a in $aliasFiles) {
        Copy-Item $a.FullName (Join-Path $defs.old $a.Name)
        $bytes = [IO.File]::ReadAllBytes($a.FullName)
        if (-not ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)) { $bytes = [byte[]](0xEF, 0xBB, 0xBF) + $bytes }
        [IO.File]::WriteAllBytes((Join-Path $defs.oldbom $a.Name), $bytes)
    }
    Copy-Item $Definition (Join-Path $defs.new 'Text2yks.txt')

    $runs = @(@('old', $oldDll), @('oldbom', $oldDll), @('new', $newDll))
    foreach ($r in $runs) {
        $name = $r[0]
        $out = & $cli --dll $r[1] --data $defs[$name] --out "$work\out_$name" --time $Data 2> "$work\log_$name.txt"
        $code = $LASTEXITCODE
        $last = ($out | Select-Object -Last 2) -join '  '
        $summary.Add("| $p | $name | $last | $code |")
        if ($code -ne 0) { $exitCode = [Math]::Max($exitCode, $code) }
        $out | Where-Object { $_ -match '^LINES-CHANGED' } | ForEach-Object { $details.Add("- $p $name 行数が変わった: $_") }
        $errs = @(Get-Content "$work\log_$name.txt" -Encoding utf8 | Where-Object { $_ -match ': (error|warning):' })
        if ($errs.Count -gt 0) { $details.Add("- $p $name のログにエラー・警告 $($errs.Count) 件（$work\log_$name.txt）"); $errs | Select-Object -First 5 | ForEach-Object { $details.Add("  - $_") } }
    }

    # 行ごとの比較
    $counts = @{ encoding = 0; tab = 0; check = 0 }
    $checkRows = New-Object System.Collections.Generic.List[string]
    $encRows = New-Object System.Collections.Generic.List[string]
    $tabRows = New-Object System.Collections.Generic.List[string]
    foreach ($file in (Get-ChildItem "$work\out_old" -Recurse -File | Sort-Object FullName)) {
        $rel = $file.FullName.Substring("$work\out_old".Length + 1)
        $in     = Read-Lines (Join-Path $Data $rel)
        $old    = Read-Lines $file.FullName
        $oldbom = Read-Lines (Join-Path "$work\out_oldbom" $rel)
        $new    = Read-Lines (Join-Path "$work\out_new" $rel)
        $newOutputs["$p|$rel"] = ($new -join "`n")
        $n = [Math]::Max([Math]::Max($old.Count, $oldbom.Count), $new.Count)
        for ($i = 0; $i -lt $n; $i++) {
            $o = if ($i -lt $old.Count) { $old[$i] } else { '<無し>' }
            $b = if ($i -lt $oldbom.Count) { $oldbom[$i] } else { '<無し>' }
            $w = if ($i -lt $new.Count) { $new[$i] } else { '<無し>' }
            $src = if ($i -lt $in.Count) { $in[$i] } else { '' }
            $where = "$rel($($i + 1))"
            if ($o -ne $b) {
                $counts.encoding++
                $encRows.Add("| $where | $(Show $src) | $(Show $o) | $(Show $w) |")
            }
            if ($b -ne $w) {
                if (($b -replace "`t", '') -eq ($w -replace "`t", '')) {
                    $counts.tab++
                    if ($tabRows.Count -lt 5) { $tabRows.Add("| $where | $(Show $b) | $(Show $w) |") }
                } else {
                    $counts.check++
                    $checkRows.Add("| $where | $(Show $src) | $(Show $b) | $(Show $w) |")
                }
            }
        }
    }
    $details.Add("")
    $details.Add("## $p")
    $details.Add("")
    $details.Add("| 分類 | 行数 |")
    $details.Add("|---|---|")
    $details.Add("| 文字コード（旧≠旧BOM） | $($counts.encoding) |")
    $details.Add("| 末尾TAB（旧BOM≠新・TAB を除けば同じ） | $($counts.tab) |")
    $details.Add("| 要確認（それ以外の 旧BOM≠新） | $($counts.check) |")
    if ($checkRows.Count -gt 0) {
        $details.Add(""); $details.Add("### 要確認"); $details.Add("")
        $details.Add("| 場所 | 入力 | 旧BOM | 新 |"); $details.Add("|---|---|---|---|")
        $checkRows | ForEach-Object { $details.Add($_) }
    }
    if ($encRows.Count -gt 0) {
        $details.Add(""); $details.Add("### 文字コード"); $details.Add("")
        $details.Add("| 場所 | 入力 | 旧 | 新 |"); $details.Add("|---|---|---|---|")
        $encRows | ForEach-Object { $details.Add($_) }
    }
    if ($tabRows.Count -gt 0) {
        $details.Add(""); $details.Add("### 末尾TAB（先頭5件）"); $details.Add("")
        $details.Add("| 場所 | 旧BOM | 新 |"); $details.Add("|---|---|---|")
        $tabRows | ForEach-Object { $details.Add($_) }
    }
}

# x86 と x64 の新しい出力が同じか
if ($Platform.Count -ge 2) {
    $diff = 0
    foreach ($k in $newOutputs.Keys) {
        if ($k.StartsWith("$($Platform[0])|")) {
            $other = $Platform[1] + '|' + $k.Substring($Platform[0].Length + 1)
            if ($newOutputs[$k] -ne $newOutputs[$other]) { $diff++ }
        }
    }
    $details.Insert(0, "- 新の $($Platform[0]) と $($Platform[1]) の出力が違うファイル: $diff")
}

$text = New-Object System.Collections.Generic.List[string]
$text.Add("# 旧 Text2yks と新 Text2yks の比較（$game）")
$text.Add("")
$text.Add("| ビット数 | 種類 | Text2yksCli の結果 | 終了コード |")
$text.Add("|---|---|---|---|")
$summary | ForEach-Object { $text.Add($_) }
$text.Add("")
$details | ForEach-Object { $text.Add($_) }
if ($Report -ne '') {
    [IO.File]::WriteAllText($Report, ($text -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($true)))
    Write-Host "書きました: $Report"
    # 画面には集計だけ出す
    $text | Where-Object { $_ -match '^# |^\| x(64|86) |^## |^\| (文字コード|末尾TAB|要確認)（|^- ' } | ForEach-Object { Write-Host $_ }
} else {
    $text | ForEach-Object { Write-Host $_ }
}
exit $exitCode
