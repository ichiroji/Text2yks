# Text2yks

Text2yks は、ノベルゲームエンジン Yuka2 がテキストのスクリプト（.yks）をコンパイルする直前に呼び出す前処理 DLL です。
定義ファイルに書いた**正規表現の置き換え規則**をスクリプトへ適用します。置き換えは行の中だけで行うため、**スクリプトの行数は決して変わりません**（コンパイルエラーの行番号が元のスクリプトとずれません）。

定義ファイルの書き方は `Document\Text2yks定義ファイル仕様.md` を参照してください。

```
// Text2yks.txt の例
:define	NAME	[A-Za-z_]\w*
@VOICE *\( *({ARG}) *, *({ARG}) *\)	SoundCreate(Layer=$1, File="VOICE\$2", Play, Stream)
```

## ビルド

| 環境 | ソリューション | 構成 | 出力 |
|---|---|---|---|
| Visual Studio 2008（Windows XP 以降・32bit） | `Text2yks.vs2008.sln` | Debug／Release（Win32） | `build\vs2008\Text2yks\x86\<構成>\Text2yks_x86.dll` |
| Visual Studio 2026（Windows 10 以降） | `Text2yks.vs2026.slnx` | Debug／Release（x86・x64） | `build\Text2yks\<x86\|x64>\<構成>\Text2yks_<x86\|x64>.dll` |

- 正規表現エンジンとして PCRE2 10.39 をソースのまま同梱し、静的ライブラリとして組み込みます（8bit・Unicode 有り・JIT 無し）。
- ソリューションには次のプロジェクトがあります。
  - `Text2yks`: 前処理 DLL
  - `Text2yksCli`: 確認用コマンド。指定した DLL を読み込み、Yuka2 と同じ手順で `ScriptPreConvert` を呼びます（`Text2yksCli --dll <DLL> --data <データフォルダ> [--out <出力フォルダ>] [--time] <.yks かフォルダ>...`）
  - `RegexProbe`: 組み込んだ PCRE2 がそのコンパイラ・ビット数で正しく動くかを確かめる確認用プログラム（全項目が通れば終了コード 0）
  - `pcre2`: PCRE2 の静的ライブラリ

## テスト

```
powershell -File tests\run_tests.ps1 -Platform x64 -Config Release
powershell -File tests\run_tests.ps1 -Toolset vs2008 -Config Release
```

`tests\cases\<名前>\` ごとに、定義（`Text2yks.txt`）・入力（`*.yks`）・期待値（`*.expected`）を置いています。

## ゲームへの置き方

Yuka2 のデータフォルダ（実行時のカレント）に次のものを置きます。

| ファイル | 内容 |
|---|---|
| `Text2yks_x64.dll` | 64bit 版（VS2026 の Release x64） |
| `Text2yks_x86.dll` | 32bit 版（Windows XP でも動かすため **VS2008 の Release**。`build\vs2008\Text2yks\x86\Release\`） |
| `Text2yks.txt` | 置き換えの定義 |
| `Text2yks_LICENSE.txt` | Text2yks（MIT）と PCRE2（BSD）のライセンス表記。DLL を配るときは一緒に配ってください |

DLL が無いときは、Yuka2 は前処理をせずにスクリプトをそのままコンパイルします（エラーにはなりません）。前処理のエラーは `OutputDebugStringW` に出ます（DebugView などで見られます）。

## 旧エイリアス定義からの移行

旧 Text2yks の `alias0.txt`〜`alias2.txt` は読みません。次のツールで `Text2yks.txt` へ変換してください。

```
powershell -File tools\alias2text2yks.ps1 -Alias <alias0.txt> [<alias1.txt> …] -Out <Text2yks.txt>
powershell -File tools\compare_old_new.ps1 -Data <ゲームフォルダ> -Definition <Text2yks.txt> -Report <差分一覧.md>
```

- `alias2text2yks.ps1`: 旧版の解析と照合を写した正規表現の規則を作ります（`名前()` は引数0個として扱う・固定値の引数・`$` の逃がし）。置き換え後の末尾の TAB は新しい形では書けないので落とし、一覧で報告します。
- `compare_old_new.ps1`: ゲームフォルダの全 .yks を、旧 DLL＋alias・旧 DLL＋BOM を付けた alias・新 DLL＋Text2yks.txt の3通りに通し、行ごとの差を分類して一覧にします（Release ビルドの Text2yksCli と DLL を使う）。

## Yuka2 との取り決め

- エクスポート関数 `bool CALLBACK ScriptPreConvert(BYTE* filename, BYTE* buff, __int64 len, HGLOBAL* out)`（無装飾名）。
- 変換したときだけ `GlobalAlloc` で確保した結果を `*out` に入れて `true` を返します。解放は Yuka2 側が行います。
- 確認用に `void CALLBACK ScriptPreConvertSetLogCallback(void (CALLBACK*)(const char* utf8message))` もエクスポートしています（ログの出し先を変える。Yuka2 は使いません）。
- DLL 名はビット数付き（`Text2yks_x86.dll`／`Text2yks_x64.dll`）で、Yuka2 のデータフォルダに置きます。

## ライセンス

Text2yks は MIT ライセンスです（`LICENSE`）。

### 第三者のソフトウェア

- **PCRE2**（`third_party\pcre2\pcre2-10.39\`）: BSD-3-Clause WITH PCRE2-exception。Copyright (c) 1997-2021 University of Cambridge（著者 Philip Hazel）。全文は `third_party\pcre2\pcre2-10.39\LICENCE` を参照してください。
  - Text2yks のビルド済み DLL を配布するときは、PCRE2 のライセンス文を添えてください。
