# 同梱している PCRE2 について

| 項目 | 内容 |
|---|---|
| 版 | 10.39（2021-10-29） |
| 取得元 | https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.39/pcre2-10.39.tar.gz |
| SHA256 | `0781bd2536ef5279b1943471fdcdbd9961a2845e1d2c9ad849b9bd98ba1a9bd4` |
| ライセンス | BSD-3-Clause WITH PCRE2-exception（`pcre2-10.39\LICENCE`） |

## この版を使う理由

- Text2yks は Visual Studio 2008（C コンパイラは C89）でもビルドする。PCRE2 は 10.43 から C99 必須になり、10.40 以降は `for (int i = …)` などの C99 構文を含むため VS2008 では通らない。C89 を保っている最後の版が 10.39。
- 全ビルド（VS2008・VS2026 x86/x64）で同じ版を使う（ビルドごとに正規表現の動きを変えないため）。
- VS2008 をやめるときは新しい版へ上げる。その場合も `pcre2-<版>\` を差し替え、プロジェクトのパスを直すだけにする。

## 写したファイル（中身は一切変えていない）

- `LICENCE`・`AUTHORS`・`COPYING`・`README`・`ChangeLog`・`NON-AUTOTOOLS-BUILD`
- `src\` のうち 8bit ライブラリに要るものだけ（NON-AUTOTOOLS-BUILD の (4) の一覧＋ヘッダー、`pcre2_jit_compile.c` が読み込む `pcre2_jit_match.c`・`pcre2_jit_misc.c`）
- NON-AUTOTOOLS-BUILD の (1)〜(3) の手順どおり、次の3つは配布物の写し（名前だけ変更・中身は同じ）
  - `config.h.generic` → `config.h`
  - `pcre2.h.generic` → `pcre2.h`
  - `pcre2_chartables.c.dist` → `pcre2_chartables.c`
- JIT（`sljit\`）・pcre2grep・pcre2test・POSIX 互換部分・テストデータ・文書は入れていない。

## ビルドの設定（`pcre2.vcproj`／`pcre2.vcxproj`）

- 静的ライブラリ `pcre2-8.lib`。C としてコンパイルする。
- 定義: `HAVE_CONFIG_H`・`PCRE2_CODE_UNIT_WIDTH=8`・`PCRE2_STATIC`・`SUPPORT_UNICODE`（`SUPPORT_JIT` は定義しない）。
- VS2008 には `<inttypes.h>` が無いので、`third_party\compat\vs2008\inttypes.h`（Text2yks の自作・MIT）を VS2008 のプロジェクトの include パスにだけ入れる（NON-AUTOTOOLS-BUILD の指示どおり。PCRE2 のソースは変えない）。

## 版を上げるときの手順

1. 新しい版の配布物（tar.gz）を取得し、SHA256 を控える。
2. `pcre2-<新しい版>\` を作り、上の「写したファイル」と同じものを写す（`config.h`・`pcre2.h`・`pcre2_chartables.c` も同じ手順で）。新しい版で必要なファイルが増えていないか NON-AUTOTOOLS-BUILD の (4) を確かめる。
3. `pcre2.vcproj`・`pcre2.vcxproj` と、`Text2yks`・`RegexProbe` のプロジェクトの include パスを新しいフォルダへ直す。
4. `tests\RegexProbe\src\RegexProbe.cpp` の版の確認（`config: version is …`）を直す。
5. 6構成（VS2026 Debug/Release × x86/x64・VS2008 Debug/Release）でビルドし、RegexProbe と `tests\run_tests.ps1` を全部通す。**VS2008 は 10.43 以降の版を C89 として通せない**（このファイルの「この版を使う理由」）。
6. 下の「使い方の注意」が新しい版でも要るかを ChangeLog で確かめる。
7. このファイルの表を直し、`tools\make_license_notice.ps1` で配布用の `Text2yks_LICENSE.txt` を作り直す。古い `pcre2-<版>\` を消す。

## 使い方の注意（Text2yks 側で守ること）

- 10.40 以降で直った不具合のうち関係しうるものを避けるため、次の使い方をしない。
  - `PCRE2_MATCH_INVALID_UTF` は使わない（10.48 で直った、不正な UTF-8 を後ろ向きに読むときの範囲外読み出し GHSA-9qww-pwc4-77qq を避ける）。不正な UTF-8 の行は Text2yks が自分で検査して PCRE2 へ渡さない。
  - パターン先頭の `(*LIMIT_HEAP=…)` などの上限指定は受け付けない（10.41 で直ったヒープ溢れ #275 を避ける）。
