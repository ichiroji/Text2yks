//---------------------------------------------------------------------------//
//  dllmain.cpp
//      DLL のエントリポイント
//---------------------------------------------------------------------------//
#include <windows.h>

void Text2yksAttach();
void Text2yksDetach();

//---------------------------------------------------------------------------//
//  BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID reserved )
//      引数 HMODULE hModule     この DLL のモジュールハンドル
//           DWORD   reason      呼ばれた理由（DLL_PROCESS_ATTACH 等）
//           LPVOID  reserved    予約
//      戻値 BOOL 常に TRUE
//      処理 読み込み時に排他とログの用意をし、解放時に後始末する。スレッドごとの通知は止める
//---------------------------------------------------------------------------//
BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID reserved )
{
	(void)reserved;
	if( reason == DLL_PROCESS_ATTACH ){
		DisableThreadLibraryCalls( hModule );
		Text2yksAttach();
	} else if( reason == DLL_PROCESS_DETACH ){
		Text2yksDetach();
	}
	return( TRUE );
}
