//---------------------------------------------------------------------------//
//  Text2yksCli.cpp
//      Text2yks の確認用コマンド
//      指定した DLL を読み込み、Yuka2 と同じ手順で ScriptPreConvert を呼ぶ（新旧の DLL の比較にも使う）
//
//      Text2yksCli --dll <DLL> --data <データフォルダ> [--out <出力フォルダ>] [--time] [--raw]
//                  [--after-first-copy <元> <先>] <.yks か フォルダ>...
//        ・カレントを --data にしてから呼ぶ（DLL は定義ファイルをカレントから読む）
//        ・入力は Yuka2 と同じく BOM を除き、BOM 無しは UTF-8 として正しければそのまま、でなければ CP932 として UTF-8 にする
//          --raw のときは変換せずにバイト列のまま渡す（不正な UTF-8 の行の確認用）
//        ・--after-first-copy は最初のファイルを処理した後に <元> を <先> へ写す（定義ファイルの読み直しの確認用。
//          パスはデータフォルダからの相対）
//        ・--out があれば結果（変わらなければ元のまま）を UTF-8（BOM 無し）で、相対パスを保って書く
//          相対パスは、フォルダで指定した .yks はそのフォルダから、ファイルで指定したものはデータフォルダから（外ならファイル名）
//        ・入力と出力の行数（改行の数）を比べ、違えば報告する
//        ・DLL に ScriptPreConvertSetLogCallback があればログを標準エラーへ出す
//      終了コード: 0＝正常・1＝行数が変わったファイルがある・2＝引数やファイル・DLL の誤り
//---------------------------------------------------------------------------//
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <algorithm>

namespace {

typedef bool (CALLBACK* ScriptPreConvertFunc)( BYTE*, BYTE*, __int64, HGLOBAL* );
typedef void (CALLBACK* LogCallbackFunc)( const char* );
typedef void (CALLBACK* SetLogCallbackFunc)( LogCallbackFunc );

//---------------------------------------------------------------------------//
//  std::wstring Utf8ToWide( const std::string& text )
//      引数 const std::string& text    UTF-8 の文字列
//      戻値 std::wstring UTF-16 にしたもの
//      処理 文字コードを変える
//---------------------------------------------------------------------------//
std::wstring Utf8ToWide( const std::string& text )
{
	if( text.empty() ) return( std::wstring() );
	int length = MultiByteToWideChar( CP_UTF8, 0, text.data(), (int)text.size(), NULL, 0 );
	if( length <= 0 ) return( std::wstring() );
	std::wstring out( (size_t)length, L'\0' );
	MultiByteToWideChar( CP_UTF8, 0, text.data(), (int)text.size(), &out[0], length );
	return( out );
}

//---------------------------------------------------------------------------//
//  std::string WideToUtf8( const wchar_t* text, size_t count )
//      引数 const wchar_t* text     UTF-16 の文字列
//           size_t         count    文字数（wchar_t の数）
//      戻値 std::string UTF-8 にしたもの
//      処理 文字コードを変える
//---------------------------------------------------------------------------//
std::string WideToUtf8( const wchar_t* text, size_t count )
{
	if( count == 0 ) return( std::string() );
	int length = WideCharToMultiByte( CP_UTF8, 0, text, (int)count, NULL, 0, NULL, NULL );
	if( length <= 0 ) return( std::string() );
	std::string out( (size_t)length, '\0' );
	WideCharToMultiByte( CP_UTF8, 0, text, (int)count, &out[0], length, NULL, NULL );
	return( out );
}

//---------------------------------------------------------------------------//
//  void CALLBACK LogToStderr( const char* message )
//      引数 const char* message    DLL のログ（UTF-8）
//      戻値 なし
//      処理 DLL のログを標準エラーへ1行で出す
//---------------------------------------------------------------------------//
void CALLBACK LogToStderr( const char* message )
{
	fwprintf( stderr, L"%s\n", Utf8ToWide( std::string( message ) ).c_str() );
}

//---------------------------------------------------------------------------//
//  std::wstring FullPath( const std::wstring& path )
//      引数 const std::wstring& path    パス
//      戻値 std::wstring 絶対パス
//      処理 相対パスを今のカレントから絶対パスにする（カレントを変える前に使う）
//---------------------------------------------------------------------------//
std::wstring FullPath( const std::wstring& path )
{
	DWORD length = GetFullPathNameW( path.c_str(), 0, NULL, NULL );
	if( length == 0 ) return( path );
	std::vector<wchar_t> buffer( length + 1 );
	GetFullPathNameW( path.c_str(), length + 1, &buffer[0], NULL );
	return( std::wstring( &buffer[0] ) );
}

//---------------------------------------------------------------------------//
//  bool IsDirectory( const std::wstring& path )
//      引数 const std::wstring& path    パス
//      戻値 bool フォルダなら true
//      処理 パスがフォルダかを調べる
//---------------------------------------------------------------------------//
bool IsDirectory( const std::wstring& path )
{
	DWORD attributes = GetFileAttributesW( path.c_str() );
	return( attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 );
}

//---------------------------------------------------------------------------//
//  bool HasYksExtension( const std::wstring& name )
//      引数 const std::wstring& name    ファイル名
//      戻値 bool 拡張子が .yks（大文字小文字を問わない）なら true
//      処理 拡張子を調べる
//---------------------------------------------------------------------------//
bool HasYksExtension( const std::wstring& name )
{
	if( name.size() < 4 ) return( false );
	std::wstring ext = name.substr( name.size() - 4 );
	for( size_t i = 0; i < ext.size(); i++ ){
		if( ext[i] >= L'A' && ext[i] <= L'Z' ) ext[i] = (wchar_t)(ext[i] - L'A' + L'a');
	}
	return( ext == L".yks" );
}

//---------------------------------------------------------------------------//
//  void CollectYks( const std::wstring& dir, std::vector<std::wstring>& files )
//      引数 const std::wstring&        dir      フォルダ（絶対パス）
//           std::vector<std::wstring>& files    見つけた .yks を足す先
//      戻値 なし
//      処理 フォルダの下（入れ子も）の .yks を集める
//---------------------------------------------------------------------------//
void CollectYks( const std::wstring& dir, std::vector<std::wstring>& files )
{
	WIN32_FIND_DATAW data;
	HANDLE find = FindFirstFileW( (dir + L"\\*").c_str(), &data );
	if( find == INVALID_HANDLE_VALUE ) return;
	do {
		std::wstring name = data.cFileName;
		if( name == L"." || name == L".." ) continue;
		std::wstring path = dir + L"\\" + name;
		if( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
			CollectYks( path, files );
		} else if( HasYksExtension( name ) ){
			files.push_back( path );
		}
	} while( FindNextFileW( find, &data ) );
	FindClose( find );
}

//---------------------------------------------------------------------------//
//  bool ReadAllBytes( const std::wstring& path, std::string& bytes )
//      引数 const std::wstring& path     パス
//           std::string&        bytes    中身
//      戻値 bool 読めたら true
//      処理 ファイルを全部読む
//---------------------------------------------------------------------------//
bool ReadAllBytes( const std::wstring& path, std::string& bytes )
{
	bytes.clear();
	FILE* fp = _wfopen( path.c_str(), L"rb" );
	if( fp == NULL ) return( false );
	char buffer[65536];
	size_t done;
	while( (done = fread( buffer, 1, sizeof(buffer), fp )) > 0 ){
		bytes.append( buffer, done );
	}
	bool ok = ( ferror( fp ) == 0 );
	fclose( fp );
	return( ok );
}

//---------------------------------------------------------------------------//
//  bool WriteAllBytes( const std::wstring& path, const std::string& bytes )
//      引数 const std::wstring& path     パス（途中のフォルダは作る）
//           const std::string&  bytes    中身
//      戻値 bool 書けたら true
//      処理 ファイルを書く
//---------------------------------------------------------------------------//
bool WriteAllBytes( const std::wstring& path, const std::string& bytes )
{
	for( size_t i = 3; i < path.size(); i++ ){
		if( path[i] == L'\\' ){
			CreateDirectoryW( path.substr( 0, i ).c_str(), NULL );
		}
	}
	FILE* fp = _wfopen( path.c_str(), L"wb" );
	if( fp == NULL ) return( false );
	bool ok = bytes.empty() || fwrite( bytes.data(), 1, bytes.size(), fp ) == bytes.size();
	if( fclose( fp ) != 0 ) ok = false;
	return( ok );
}

//---------------------------------------------------------------------------//
//  void DecodeLikeEngine( const std::string& bytes, std::string& utf8 )
//      引数 const std::string& bytes    ファイルの中身
//           std::string&       utf8     UTF-8 にしたもの（BOM は除く）
//      戻値 なし
//      処理 Yuka2 の TextToUTF8（文字コード自動判定）と同じように UTF-8 にする
//---------------------------------------------------------------------------//
void DecodeLikeEngine( const std::string& bytes, std::string& utf8 )
{
	const unsigned char* p = (const unsigned char*)bytes.data();
	size_t length = bytes.size();
	if( length >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF ){
		utf8.assign( bytes, 3, std::string::npos );
		return;
	}
	if( length >= 2 && ((p[0] == 0xFF && p[1] == 0xFE) || (p[0] == 0xFE && p[1] == 0xFF)) ){
		bool bigendian = ( p[0] == 0xFE );
		std::wstring wide;
		for( size_t i = 2; i + 1 < length; i += 2 ){
			wide += (wchar_t)( bigendian ? ((p[i] << 8) | p[i + 1]) : (p[i] | (p[i + 1] << 8)) );
		}
		utf8 = WideToUtf8( wide.data(), wide.size() );
		return;
	}
	if( length == 0 || MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)length, NULL, 0 ) > 0 ){
		utf8 = bytes;
		return;
	}
	int count = MultiByteToWideChar( 932, 0, bytes.data(), (int)length, NULL, 0 );
	std::wstring wide( (size_t)(count > 0 ? count : 0), L'\0' );
	if( count > 0 ){
		MultiByteToWideChar( 932, 0, bytes.data(), (int)length, &wide[0], count );
	}
	utf8 = WideToUtf8( wide.data(), wide.size() );
}

//---------------------------------------------------------------------------//
//  size_t CountLines( const std::string& text )
//      引数 const std::string& text    テキスト
//      戻値 size_t 行数（改行の数＋1。\r\n・\n・\r を1つの改行と数える）
//      処理 行数を数える
//---------------------------------------------------------------------------//
size_t CountLines( const std::string& text )
{
	size_t lines = 1;
	for( size_t i = 0; i < text.size(); i++ ){
		if( text[i] == '\r' ){
			lines++;
			if( i + 1 < text.size() && text[i + 1] == '\n' ) i++;
		} else if( text[i] == '\n' ){
			lines++;
		}
	}
	return( lines );
}

//---------------------------------------------------------------------------//
//  void PrintUsage()
//      引数 なし
//      戻値 なし
//      処理 使い方を出す
//---------------------------------------------------------------------------//
void PrintUsage()
{
	fwprintf( stderr, L"usage: Text2yksCli --dll <dll> --data <data folder> [--out <output folder>] [--time] [--raw]\n"
		L"                   [--after-first-copy <src> <dst>] <.yks file or folder>...\n" );
}

} // namespace

//---------------------------------------------------------------------------//
//  int wmain( int argc, wchar_t* argv[] )
//      引数 int      argc    引数の数
//           wchar_t* argv[]  引数
//      戻値 int 0＝正常・1＝行数が変わったファイルがある・2＝引数やファイル・DLL の誤り
//      処理 DLL を読み込み、指定された .yks を順に ScriptPreConvert へ通す
//---------------------------------------------------------------------------//
int wmain( int argc, wchar_t* argv[] )
{
	_setmode( _fileno( stdout ), _O_U8TEXT );
	_setmode( _fileno( stderr ), _O_U8TEXT );

	std::wstring dll;
	std::wstring data;
	std::wstring out;
	bool showtime = false;
	bool raw      = false;
	std::wstring copysource;
	std::wstring copytarget;
	std::vector<std::wstring> inputs;
	for( int i = 1; i < argc; i++ ){
		std::wstring arg = argv[i];
		if( arg == L"--dll" && i + 1 < argc ){
			dll = argv[++i];
		} else if( arg == L"--data" && i + 1 < argc ){
			data = argv[++i];
		} else if( arg == L"--out" && i + 1 < argc ){
			out = argv[++i];
		} else if( arg == L"--time" ){
			showtime = true;
		} else if( arg == L"--raw" ){
			raw = true;
		} else if( arg == L"--after-first-copy" && i + 2 < argc ){
			copysource = argv[++i];
			copytarget = argv[++i];
		} else if( arg.size() >= 2 && arg[0] == L'-' && arg[1] == L'-' ){
			fwprintf( stderr, L"unknown option: %s\n", arg.c_str() );
			PrintUsage();
			return( 2 );
		} else {
			inputs.push_back( arg );
		}
	}
	if( dll.empty() || data.empty() || inputs.empty() ){
		PrintUsage();
		return( 2 );
	}

	// カレントを変える前に絶対パスにする
	dll  = FullPath( dll );
	data = FullPath( data );
	if( !out.empty() ) out = FullPath( out );
	// 相対パス（ログの名前・--out の書き先）: フォルダで指定したものはそのフォルダから、
	// ファイルで指定したものはデータフォルダの下ならそこから、それ以外はファイル名だけ
	std::wstring dataprefix = data;
	if( !dataprefix.empty() && dataprefix[dataprefix.size() - 1] != L'\\' ) dataprefix += L'\\';
	std::vector<std::wstring> files;
	std::vector<std::wstring> relatives;
	for( size_t i = 0; i < inputs.size(); i++ ){
		std::wstring path = FullPath( inputs[i] );
		if( IsDirectory( path ) ){
			std::vector<std::wstring> found;
			CollectYks( path, found );
			std::sort( found.begin(), found.end() );
			size_t skip = path.size() + ( path[path.size() - 1] == L'\\' ? 0 : 1 );
			for( size_t k = 0; k < found.size(); k++ ){
				files.push_back( found[k] );
				relatives.push_back( found[k].substr( skip ) );
			}
		} else {
			files.push_back( path );
			if( path.size() > dataprefix.size() && CompareStringW( LOCALE_INVARIANT, NORM_IGNORECASE,
				path.c_str(), (int)dataprefix.size(), dataprefix.c_str(), (int)dataprefix.size() ) == CSTR_EQUAL ){
				relatives.push_back( path.substr( dataprefix.size() ) );
			} else {
				size_t slash = path.find_last_of( L'\\' );
				relatives.push_back( slash == std::wstring::npos ? path : path.substr( slash + 1 ) );
			}
		}
	}

	HMODULE module = LoadLibraryW( dll.c_str() );
	if( module == NULL ){
		fwprintf( stderr, L"cannot load DLL: %s (error %lu)\n", dll.c_str(), GetLastError() );
		return( 2 );
	}
	ScriptPreConvertFunc convert = (ScriptPreConvertFunc)GetProcAddress( module, "ScriptPreConvert" );
	if( convert == NULL ){
		fwprintf( stderr, L"ScriptPreConvert is not exported: %s\n", dll.c_str() );
		return( 2 );
	}
	SetLogCallbackFunc setlog = (SetLogCallbackFunc)GetProcAddress( module, "ScriptPreConvertSetLogCallback" );
	if( setlog != NULL ){
		setlog( LogToStderr );
	}
	if( !SetCurrentDirectoryW( data.c_str() ) ){
		fwprintf( stderr, L"cannot change directory: %s\n", data.c_str() );
		return( 2 );
	}

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency( &frequency );
	LONGLONG elapsed = 0;
	size_t converted = 0;
	size_t mismatched = 0;
	size_t failed = 0;

	for( size_t f = 0; f < files.size(); f++ ){
		const std::wstring& path     = files[f];
		const std::wstring& relative = relatives[f];

		std::string bytes;
		if( !ReadAllBytes( path, bytes ) ){
			fwprintf( stderr, L"cannot read: %s\n", path.c_str() );
			failed++;
			continue;
		}
		std::string text;
		if( raw ){
			text = bytes;
		} else {
			DecodeLikeEngine( bytes, text );
		}

		std::string result = text;
		bool changed = false;
		if( !text.empty() ){
			std::string name = WideToUtf8( relative.data(), relative.size() );
			HGLOBAL memory = NULL;
			LARGE_INTEGER start;
			LARGE_INTEGER end;
			QueryPerformanceCounter( &start );
			bool ret = convert( (BYTE*)name.c_str(), (BYTE*)&text[0], (__int64)text.size(), &memory );
			QueryPerformanceCounter( &end );
			elapsed += end.QuadPart - start.QuadPart;
			if( ret && memory != NULL ){
				size_t size = GlobalSize( memory );
				result.assign( size, '\0' );
				if( size > 0 ){
					void* p = GlobalLock( memory );
					if( p != NULL ){
						memcpy( &result[0], p, size );
						GlobalUnlock( memory );
					}
				}
				changed = true;
			}
			if( memory != NULL ){
				GlobalFree( memory );
			}
		}
		if( changed ) converted++;

		size_t linesin  = CountLines( text );
		size_t linesout = CountLines( result );
		if( linesin != linesout ){
			mismatched++;
			fwprintf( stdout, L"LINES-CHANGED %s (%u -> %u)\n", relative.c_str(), (unsigned int)linesin, (unsigned int)linesout );
		} else {
			fwprintf( stdout, L"%s %s\n", changed ? L"converted" : L"unchanged", relative.c_str() );
		}
		if( !out.empty() ){
			std::wstring target = out + L"\\" + relative;
			if( !WriteAllBytes( target, result ) ){
				fwprintf( stderr, L"cannot write: %s\n", target.c_str() );
				failed++;
			}
		}
		if( f == 0 && !copysource.empty() ){
			if( !CopyFileW( copysource.c_str(), copytarget.c_str(), FALSE ) ){
				fwprintf( stderr, L"cannot copy: %s -> %s\n", copysource.c_str(), copytarget.c_str() );
				failed++;
			}
		}
	}

	fwprintf( stdout, L"files=%u converted=%u lines-changed=%u errors=%u\n",
		(unsigned int)files.size(), (unsigned int)converted, (unsigned int)mismatched, (unsigned int)failed );
	if( showtime ){
		fwprintf( stdout, L"dll-time=%.1fms\n", (double)elapsed * 1000.0 / (double)frequency.QuadPart );
	}
	if( setlog != NULL ){
		setlog( NULL );
	}
	if( failed > 0 ) return( 2 );
	if( mismatched > 0 ) return( 1 );
	return( 0 );
}
