# 旧 Text2yks のエイリアス定義（alias0.txt〜alias2.txt）を、新しい Text2yks.txt へ変換する
#
# 旧版（Text2yks.old）の解析と照合を忠実に写す:
#   ・行のどこかに // があればそこから後はコメント。最初の TAB までが名前、TAB の並びの後が置き換え（前後の空白も含む）
#   ・名前が「命令(引数,…)」なら引数あり。% で始まる引数は差し込み、それ以外は固定値（末尾の , は空の固定値）
#     「命令()」は引数ではなく文字どおりの一致
#   ・照合では「名前()」「名前( )」は引数0個。引数1個の規則には当てない
# 新しい形:
#   単純        TEXT                → 正規表現の特別な文字を \ で逃がしたもの
#   引数1個     命令(%1)            → 命令 *\( *+(?!\))({ARG}) *\)
#   引数n個     命令(%1,%2,…)       → 命令 *\( *({ARG}) *, *({ARG}) … *\)
#   固定値      命令(%1,C)          → その位置に逃がした C（グループは作らない）
#   置き換え    $ → $$、差し込み名を引数の順に $1・$2… へ（旧版と同じく先の名前から順に置き換える）
#   置き換えの末尾の TAB は新しい形では書けないので落とす（空白だけなら残す）。落としたものは一覧で報告する
#
# 例: powershell -File tools\alias2text2yks.ps1 -Alias 'D:\Game\alias0.txt' -Out 'D:\Game\Text2yks.txt'
# 終了コード: 0＝変換できた・1＝変換できない行があった（その行は出力しない）
param(
    [Parameter(Mandatory = $true)]
    [string[]]$Alias,               # 旧版が読む順（alias0・alias1・alias2）に並べる
    [Parameter(Mandatory = $true)]
    [string]$Out
)
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)    # 不正なバイトで例外にする

# 正規表現の特別な文字を逃がす（{ も逃がすので {NAME} の展開も起きない）
function Escape-Pattern([string]$text) {
    $out = [regex]::Replace($text, '[\\^$.|?*+()\[\]{}]', { param($m) '\' + $m.Value })
    if ($out.StartsWith(':')) { $out = '\' + $out }
    return $out
}

# 旧版の引数の解析（classAliasCommand::OneLineanAlysis と同じ）
function Parse-Params([string]$name) {
    $params = New-Object System.Collections.Generic.List[string]
    $depth = 0; $i = 0; $paraflag = $false
    while ($i -lt $name.Length) {
        $c = $name[$i]
        if ($c -eq '(') { $depth++; $i++ }
        elseif ($c -eq ')') { $depth--; $i++ }
        else {
            $paraflag = $false
            if ($depth -eq 1) {
                $found = $name.IndexOfAny([char[]]@(',', ')'), $i)
                if ($found -ge 0) {
                    if ($found -gt $i) { $params.Add($name.Substring($i, $found - $i).Trim(' ')) } else { $params.Add('') }
                    $i = $found
                    if ($name[$i] -eq ',') { $paraflag = $true; $i++ }
                } else { $i++ }
            } else { $i++ }
        }
    }
    if ($paraflag) { $params.Add('') }
    if ($params.Count -gt 0 -and $depth -eq 0) {
        return @{ Command = $name.Substring(0, $name.IndexOf('(')); Params = $params.ToArray() }
    }
    return @{ Command = $name; Params = @() }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('// Text2yks.txt')
$lines.Add('// ' + (($Alias | ForEach-Object { Split-Path $_ -Leaf }) -join '・') + ' から tools\alias2text2yks.ps1 で変換（' + (Get-Date -Format 'yyyy-MM-dd') + '）')
$lines.Add('// 書き方は Text2yks の Document\Text2yks定義ファイル仕様.md を参照')
$lines.Add('')

$rules = 0; $errors = 0
$droppedTabs = New-Object System.Collections.Generic.List[string]
foreach ($file in $Alias) {
    $leaf = Split-Path $file -Leaf
    $bytes = [IO.File]::ReadAllBytes($file)
    $start = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $start = 3 }
    try {
        $text = $utf8.GetString($bytes, $start, $bytes.Length - $start)
    } catch {
        # 本当に CP932 で書かれたものは CP932 として読む
        Write-Host "$leaf : UTF-8 ではないので CP932 として読みます"
        $text = [Text.Encoding]::GetEncoding(932).GetString($bytes)
    }
    $lines.Add("// ---- $leaf ----")

    $lineNo = 0
    foreach ($raw in ($text -split "`r`n|`n|`r")) {
        $lineNo++
        if ($raw -eq '') { continue }
        $line = $raw
        $comment = $null
        $c = $line.IndexOf('//')
        if ($c -ge 0) { $comment = $line.Substring($c + 2); $line = $line.Substring(0, $c) }

        $tab = $line.IndexOf("`t")
        if ($tab -lt 0) { $name = $line; $dest = '' } else { $name = $line.Substring(0, $tab); $dest = $line.Substring($tab).TrimStart("`t") }
        if ($name -eq '') {
            if ($null -ne $comment) { $lines.Add('//' + $comment) }
            continue
        }
        if ($name.Trim(" `t") -eq '') {
            Write-Host "$leaf($lineNo): 名前が空白だけの規則は変換できません: [$raw]"
            $errors++; continue
        }
        if ($tab -lt 0) { Write-Host "$leaf($lineNo): (注意) TAB が無いので空文字への置き換えになります: [$raw]" }

        # 置き換えの末尾の TAB（新しい形では書けない）
        $m = [regex]::Match($dest, '[ \t]+$')
        if ($m.Success -and $m.Value.Contains("`t")) {
            $dest = $dest.Substring(0, $m.Index)
            $droppedTabs.Add("$leaf($lineNo): $name")
        }
        if ($dest.Contains("`t")) {
            Write-Host "$leaf($lineNo): 置き換えの途中に TAB があるので変換できません: [$raw]"
            $errors++; continue
        }

        $parsed = Parse-Params $name
        $replacement = $dest.Replace('$', '$$')
        if ($parsed.Params.Count -eq 0) {
            $pattern = Escape-Pattern $name
        } else {
            $slots = @()
            $group = 0
            $markers = @()
            foreach ($p in $parsed.Params) {
                if ($p.StartsWith('%')) {
                    $group++
                    $slots += '({ARG})'
                    $markers += , @($p, $group)
                } else {
                    $slots += (Escape-Pattern $p)
                }
            }
            $cmd = Escape-Pattern $parsed.Command
            if ($parsed.Params.Count -eq 1 -and $group -eq 1) {
                $pattern = $cmd + ' *\( *+(?!\))({ARG}) *\)'
            } else {
                $pattern = $cmd + ' *\( *' + ($slots -join ' *, *') + ' *\)'
            }
            # 差し込み名を順に印へ置き換えてから $k にする（旧版と同じ順序。値の中身は見ない）
            foreach ($mk in $markers) { $replacement = $replacement.Replace($mk[0], [string][char]1 + $mk[1] + [char]2) }
            $replacement = [regex]::Replace($replacement, "\x01(\d+)\x02", '$$$1')
            if ($group -ge 10 -and [regex]::IsMatch($replacement, '\$\d\d')) {
                Write-Host "$leaf($lineNo): 差し込みが10個以上あり、`$n の後に数字が続くので読み違えが起きます: [$raw]"
                $errors++; continue
            }
        }

        $note = "// $name"
        if ($null -ne $comment -and $comment.Trim() -ne '') { $note += '  ' + $comment.Trim() }
        if ($replacement -eq '') {
            $lines.Add($note)
            $lines.Add($pattern + "`t")
        } else {
            $lines.Add($pattern + "`t" + $replacement + "`t" + $note)
        }
        $rules++
    }
    $lines.Add('')
}

[IO.File]::WriteAllText($Out, ($lines -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($true)))
Write-Host "規則 $rules 個を書きました: $Out"
if ($droppedTabs.Count -gt 0) {
    Write-Host "置き換えの末尾の TAB を落とした規則 ($($droppedTabs.Count) 個):"
    $droppedTabs | ForEach-Object { Write-Host "  $_" }
}
if ($errors -gt 0) { Write-Host "変換できなかった行: $errors"; exit 1 }
exit 0
