//---------------------------------------------------------------------------//
//  Text2yks.cpp
//      エクスポート関数 ScriptPreConvert
//---------------------------------------------------------------------------//
#include "Text2yks.h"

//---------------------------------------------------------------------------//
//  bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem )
//      引数 BYTE*    filename      変換対象のファイル名（UTF-8）
//           BYTE*    buff          変換対象のスクリプト文字列（UTF-8）
//           __int64  filelength    スクリプト文字列の長さ（バイト）
//           HGLOBAL* pHGmem        変換後の文字列を GlobalAlloc で確保して返す先（呼び出し側が GlobalFree する）
//      戻値 bool 変換した場合 true、変換しなかった場合 false
//      処理 工程0の雛形。まだ何も変換せず false を返す（DLL が無いときと同じ動き）
//---------------------------------------------------------------------------//
bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem )
{
	(void)filename;
	(void)buff;
	(void)filelength;
	(void)pHGmem;
	return( false );
}
