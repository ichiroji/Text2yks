//---------------------------------------------------------------------------//
//  Text2yks.h
//      Yuka2 がテキストのスクリプト(.yks)をコンパイルする直前に呼ぶ前処理 DLL の入口
//      ScriptPreConvert のエクスポート名・引数・戻り値・メモリの受け渡し（GlobalAlloc で確保し、呼び出し側が GlobalFree する）は
//      Yuka2 側（Yuka2\src\YukaScript.cpp）との契約なので変えないこと
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_TEXT2YKS_H
#define TEXT2YKS_TEXT2YKS_H

#include <windows.h>

// ログを受け取る関数。message は UTF-8（"Text2yks: error: …" の形・改行は含まない）
typedef void (CALLBACK* Text2yksLogCallback)( const char* message );

// エクスポート名は Text2yks.def で無装飾にする
bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem );
// 確認用（Text2yksCli が使う。Yuka2 は呼ばない）。NULL で既定（OutputDebugStringW）へ戻す
void CALLBACK ScriptPreConvertSetLogCallback( Text2yksLogCallback callback );

#endif // TEXT2YKS_TEXT2YKS_H
