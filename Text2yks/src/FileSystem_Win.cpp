//---------------------------------------------------------------------------//
//  FileSystem_Win.cpp
//      FileSystem の Windows 版
//---------------------------------------------------------------------------//
#include "FileSystem_Win.h"
#include <windows.h>

namespace Text2yks {

//---------------------------------------------------------------------------//
//  std::wstring Utf8ToWide( const std::string& text )
//      引数 const std::string& text    UTF-8 の文字列
//      戻値 std::wstring UTF-16 にしたもの（不正なバイトは U+FFFD になる）
//      処理 パスやログを Win32 API へ渡すために UTF-16 にする
//---------------------------------------------------------------------------//
std::wstring Utf8ToWide( const std::string& text )
{
	if( text.empty() ){
		return( std::wstring() );
	}
	int length = MultiByteToWideChar( CP_UTF8, 0, text.data(), (int)text.size(), NULL, 0 );
	if( length <= 0 ){
		return( std::wstring() );
	}
	std::wstring out( (size_t)length, L'\0' );
	MultiByteToWideChar( CP_UTF8, 0, text.data(), (int)text.size(), &out[0], length );
	return( out );
}

//---------------------------------------------------------------------------//
//  bool FileSystemWin::ReadAll( const std::string& path, std::string& bytes )
//      引数 const std::string& path     パス（UTF-8）
//           std::string&       bytes    ファイルの中身
//      戻値 bool 読めたら true
//      処理 ファイルの中身を全部読む
//---------------------------------------------------------------------------//
bool FileSystemWin::ReadAll( const std::string& path, std::string& bytes )
{
	bytes.clear();
	HANDLE file = CreateFileW( Utf8ToWide( path ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( file == INVALID_HANDLE_VALUE ){
		return( false );
	}
	bool ok = true;
	LARGE_INTEGER size;
	if( !GetFileSizeEx( file, &size ) || size.HighPart != 0 || size.LowPart > 0x7FFFFFFF ){
		ok = false;
	} else if( size.LowPart > 0 ){
		bytes.resize( (size_t)size.LowPart );
		DWORD done = 0;
		if( !ReadFile( file, &bytes[0], size.LowPart, &done, NULL ) || done != size.LowPart ){
			ok = false;
			bytes.clear();
		}
	}
	CloseHandle( file );
	return( ok );
}

//---------------------------------------------------------------------------//
//  FileStamp FileSystemWin::GetStamp( const std::string& path )
//      引数 const std::string& path    パス（UTF-8）
//      戻値 FileStamp ファイルの有無・大きさ・更新日時
//      処理 定義ファイルが変わったかを見るための印を得る
//---------------------------------------------------------------------------//
FileStamp FileSystemWin::GetStamp( const std::string& path )
{
	FileStamp stamp;
	WIN32_FILE_ATTRIBUTE_DATA data;
	if( GetFileAttributesExW( Utf8ToWide( path ).c_str(), GetFileExInfoStandard, &data )
		&& (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ){
		stamp.exists = true;
		stamp.size   = ((unsigned long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
		stamp.time   = ((unsigned long long)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
	}
	return( stamp );
}

} // namespace Text2yks
