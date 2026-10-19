//---------------------------------------------------------------------------//
//  dllmain.cpp
//      DLL のエントリポイント
//---------------------------------------------------------------------------//
#include <windows.h>

//---------------------------------------------------------------------------//
//  BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID reserved )
//      引数 HMODULE hModule     この DLL のモジュールハンドル
//           DWORD   reason      呼ばれた理由（DLL_PROCESS_ATTACH 等）
//           LPVOID  reserved    予約
//      戻値 BOOL 常に TRUE
//      処理 スレッドごとの通知は不要なので止める。それ以外は何もしない
//---------------------------------------------------------------------------//
BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID reserved )
{
	(void)reserved;
	if( reason == DLL_PROCESS_ATTACH ){
		DisableThreadLibraryCalls( hModule );
	}
	return( TRUE );
}
