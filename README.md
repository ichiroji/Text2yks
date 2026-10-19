# Text2yks

Text2yks は、ノベルゲームエンジン Yuka2 がテキストのスクリプト（.yks）をコンパイルする直前に呼び出す前処理 DLL です。
定義ファイルに書いた**正規表現の置き換え規則**をスクリプトへ適用します。置き換えは行の中だけで行うため、**スクリプトの行数は決して変わりません**（コンパイルエラーの行番号が元のスクリプトとずれません）。

> 開発中です。定義ファイルの仕様は `Document\Text2yks定義ファイル仕様.md`（作成予定）にまとめます。

## ビルド

| 環境 | ソリューション | 構成 | 出力 |
|---|---|---|---|
| Visual Studio 2008（Windows XP 以降・32bit） | `Text2yks.vs2008.sln` | Debug／Release（Win32） | `build\Text2yks\x86\<構成>\Text2yks_x86.dll` |
| Visual Studio 2026（Windows 10 以降） | `Text2yks.vs2026.slnx` | Debug／Release（x86・x64） | `build\Text2yks\<x86\|x64>\<構成>\Text2yks_<x86\|x64>.dll` |

- 正規表現エンジンとして PCRE2 10.39 をソースのまま同梱し、静的ライブラリとして組み込みます（8bit・Unicode 有り・JIT 無し）。
- `tests\RegexProbe` は、組み込んだ PCRE2 がそのコンパイラ・ビット数で正しく動くかを確かめる確認用プログラムです（全項目が通れば終了コード 0）。

## Yuka2 との取り決め

- エクスポート関数 `bool CALLBACK ScriptPreConvert(BYTE* filename, BYTE* buff, __int64 len, HGLOBAL* out)`（無装飾名）。
- 変換したときだけ `GlobalAlloc` で確保した結果を `*out` に入れて `true` を返します。解放は Yuka2 側が行います。
- DLL 名はビット数付き（`Text2yks_x86.dll`／`Text2yks_x64.dll`）で、Yuka2 のデータフォルダに置きます。

## ライセンス

Text2yks は MIT ライセンスです（`LICENSE`）。

### 第三者のソフトウェア

- **PCRE2**（`third_party\pcre2\pcre2-10.39\`）: BSD-3-Clause WITH PCRE2-exception。Copyright (c) 1997-2021 University of Cambridge（著者 Philip Hazel）。全文は `third_party\pcre2\pcre2-10.39\LICENCE` を参照してください。
  - Text2yks のビルド済み DLL を配布するときは、PCRE2 のライセンス文を添えてください。
