//---------------------------------------------------------------------------//
//  Text2yks.cpp
//      エクスポート関数（定義ファイルの読み込みと保持・結果の GlobalAlloc）
//---------------------------------------------------------------------------//
#include "Text2yks.h"
#include <string.h>
#include <string>
#include <vector>
#include "FileSystem_Win.h"
#include "Log.h"
#include "RuleFile.h"
#include "RuleSet.h"

using namespace Text2yks;

namespace {
const char* const DefinitionFileName = "Text2yks.txt";   // カレント（データフォルダ）からの相対名

CRITICAL_SECTION         Lock;
FileSystemWin            Files;
RuleSet                  CurrentRules;
bool                     Loaded = false;
std::vector<std::string> LoadedFiles;       // 読んだ（読もうとした）定義ファイル
std::vector<FileStamp>   LoadedStamps;      // 読んだときの印
Text2yksLogCallback      Callback = NULL;

//---------------------------------------------------------------------------//
//  void DllLogSink( const char* message, void* context )
//      引数 const char* message    ログ（UTF-8）
//           void*       context    使わない
//      戻値 なし
//      処理 ログを出す。コールバックが設定されていればそちらへ、無ければ OutputDebugStringW へ
//---------------------------------------------------------------------------//
void DllLogSink( const char* message, void* context )
{
	(void)context;
	if( Callback != NULL ){
		Callback( message );
		return;
	}
	std::wstring text = Utf8ToWide( std::string( message ) + "\n" );
	OutputDebugStringW( text.c_str() );
}

//---------------------------------------------------------------------------//
//  void ReloadIfChanged()
//      引数 なし
//      戻値 なし
//      処理 初回、または定義ファイル（:include 先を含む）の有無・大きさ・更新日時が変わっていれば読み直す
//---------------------------------------------------------------------------//
void ReloadIfChanged()
{
	bool reload = !Loaded;
	for( size_t i = 0; !reload && i < LoadedFiles.size(); i++ ){
		if( Files.GetStamp( LoadedFiles[i] ) != LoadedStamps[i] ){
			reload = true;
		}
	}
	if( !reload ){
		return;
	}
	std::vector<RuleSource> sources;
	RuleFileReader reader( Files );
	bool ok = reader.Load( DefinitionFileName, sources, LoadedFiles );
	LoadedStamps.clear();
	for( size_t i = 0; i < LoadedFiles.size(); i++ ){
		LoadedStamps.push_back( Files.GetStamp( LoadedFiles[i] ) );
	}
	CurrentRules.Build( sources );
	Loaded = true;
	if( ok ){
		LogInfo( std::string( "loaded " ) + NumberToString( CurrentRules.RuleCount() ) + " rule(s) from "
			+ NumberToString( LoadedFiles.size() ) + " file(s) (" + DefinitionFileName + ")" );
	}
}
} // namespace

//---------------------------------------------------------------------------//
//  void Text2yksAttach() / void Text2yksDetach()
//      引数 なし
//      戻値 なし
//      処理 DllMain から呼ぶ。排他の用意と後始末・ログの出し先の設定
//---------------------------------------------------------------------------//
void Text2yksAttach()
{
	InitializeCriticalSection( &Lock );
	SetLogSink( DllLogSink, NULL );
}
void Text2yksDetach()
{
	SetLogSink( NULL, NULL );
	DeleteCriticalSection( &Lock );
}

//---------------------------------------------------------------------------//
//  bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem )
//      引数 BYTE*    filename      変換対象のファイル名（UTF-8。ログにだけ使う）
//           BYTE*    buff          変換対象のスクリプト文字列（UTF-8）
//           __int64  filelength    スクリプト文字列の長さ（バイト）
//           HGLOBAL* pHGmem        変換後の文字列を GlobalAlloc で確保して返す先（呼び出し側が GlobalFree する）
//      戻値 bool 変換した場合 true、変換しなかった場合 false（このとき pHGmem は触らない）
//      処理 定義ファイルの規則でスクリプトを置き換える。行数は変わらない
//           定義ファイルは初回と、変わったときに読み直す
//---------------------------------------------------------------------------//
bool CALLBACK ScriptPreConvert( BYTE* filename, BYTE* buff, __int64 filelength, HGLOBAL* pHGmem )
{
	if( buff == NULL || pHGmem == NULL || filelength <= 0 || filelength > 0x7FFFFFFF ){
		return( false );
	}
	bool converted = false;
	EnterCriticalSection( &Lock );
	try {
		ReloadIfChanged();
		std::string name = ( filename != NULL ) ? std::string( (const char*)filename ) : std::string();
		std::string out;
		if( CurrentRules.Apply( (const char*)buff, (size_t)filelength, name, out ) ){
			HGLOBAL memory = GlobalAlloc( GHND, (SIZE_T)out.size() );
			if( memory == NULL ){
				LogError( LogWhere( name, 0 ) + "out of memory; the script is passed through unchanged" );
			} else if( out.empty() ){
				// 全部消えた（旧版と同じく大きさ0の領域を返す）
				*pHGmem = memory;
				converted = true;
			} else {
				void* p = GlobalLock( memory );
				if( p == NULL ){
					GlobalFree( memory );
					LogError( LogWhere( name, 0 ) + "GlobalLock failed; the script is passed through unchanged" );
				} else {
					memcpy( p, out.data(), out.size() );
					GlobalUnlock( memory );
					*pHGmem = memory;
					converted = true;
				}
			}
		}
	} catch( ... ){
		LogError( "unexpected exception; the script is passed through unchanged" );
		converted = false;
	}
	LeaveCriticalSection( &Lock );
	return( converted );
}

//---------------------------------------------------------------------------//
//  void CALLBACK ScriptPreConvertSetLogCallback( Text2yksLogCallback callback )
//      引数 Text2yksLogCallback callback    ログを受け取る関数（NULL なら OutputDebugStringW へ戻す）
//      戻値 なし
//      処理 ログの出し先を設定する（確認用。Yuka2 は呼ばない）
//---------------------------------------------------------------------------//
void CALLBACK ScriptPreConvertSetLogCallback( Text2yksLogCallback callback )
{
	EnterCriticalSection( &Lock );
	Callback = callback;
	LeaveCriticalSection( &Lock );
}
