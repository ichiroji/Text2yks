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
