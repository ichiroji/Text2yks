//---------------------------------------------------------------------------//
//  Text2yks.h
//      Yuka2 がテキストのスクリプト(.yks)をコンパイルする直前に呼ぶ前処理 DLL の入口
//      エクスポート名・引数・戻り値・メモリの受け渡し（GlobalAlloc で確保し、呼び出し側が GlobalFree する）は
//      Yuka2 側（Yuka2\src\YukaScript.cpp）との契約なので変えないこと
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_TEXT2YKS_H
#define TEXT2YKS_TEXT2YKS_H

#include <windows.h>

// エクスポート名は Text2yks.def で無装飾の ScriptPreConvert にする
bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem );

#endif // TEXT2YKS_TEXT2YKS_H
