//---------------------------------------------------------------------------//
//  Log.cpp
//      ログの受け口
//---------------------------------------------------------------------------//
#include "Log.h"

namespace Text2yks {

namespace {
LogSink CurrentSink    = NULL;
void*   CurrentContext = NULL;

//---------------------------------------------------------------------------//
//  void Emit( const char* level, const std::string& message )
//      引数 const char*        level      "error"・"warning"・"info"
//           const std::string& message    内容
//      戻値 なし
//      処理 「Text2yks: <level>: <内容>」の形にして受け口へ渡す。受け口が無ければ捨てる
//---------------------------------------------------------------------------//
void Emit( const char* level, const std::string& message )
{
	if( CurrentSink == NULL ){
		return;
	}
	std::string text = "Text2yks: ";
	text += level;
	text += ": ";
	text += message;
	CurrentSink( text.c_str(), CurrentContext );
}
} // namespace

//---------------------------------------------------------------------------//
//  void SetLogSink( LogSink sink, void* context )
//      引数 LogSink sink       ログの出し先（NULL なら出さない）
//           void*   context    sink へそのまま渡す値
//      戻値 なし
//      処理 ログの出し先を設定する
//---------------------------------------------------------------------------//
void SetLogSink( LogSink sink, void* context )
{
	CurrentSink    = sink;
	CurrentContext = context;
}

//---------------------------------------------------------------------------//
//  void LogError( const std::string& message )
//      引数 const std::string& message    内容（英語）
//      戻値 なし
//      処理 エラーを出す
//---------------------------------------------------------------------------//
void LogError( const std::string& message )
{
	Emit( "error", message );
}

//---------------------------------------------------------------------------//
//  void LogWarning( const std::string& message )
//      引数 const std::string& message    内容（英語）
//      戻値 なし
//      処理 警告を出す
//---------------------------------------------------------------------------//
void LogWarning( const std::string& message )
{
	Emit( "warning", message );
}

//---------------------------------------------------------------------------//
//  void LogInfo( const std::string& message )
//      引数 const std::string& message    内容（英語）
//      戻値 なし
//      処理 お知らせ（定義を読んだ等）を出す
//---------------------------------------------------------------------------//
void LogInfo( const std::string& message )
{
	Emit( "info", message );
}

//---------------------------------------------------------------------------//
//  std::string LogWhere( const std::string& file, int line )
//      引数 const std::string& file    ファイル名
//           int                line    行番号（1〜。0 以下なら行番号を付けない）
//      戻値 std::string 「file(line): 」の形の文字列
//      処理 ログの先頭に付ける場所の表記を作る
//---------------------------------------------------------------------------//
std::string LogWhere( const std::string& file, int line )
{
	std::string text = file;
	if( line > 0 ){
		text += "(";
		text += NumberToString( (size_t)line );
		text += ")";
	}
	text += ": ";
	return( text );
}

//---------------------------------------------------------------------------//
//  std::string NumberToString( size_t value )
//      引数 size_t value    数
//      戻値 std::string 10進の文字列
//      処理 数を文字列にする（ロケールに左右されない）
//---------------------------------------------------------------------------//
std::string NumberToString( size_t value )
{
	char buffer[32];
	size_t pos = sizeof(buffer);
	buffer[--pos] = '\0';
	do {
		buffer[--pos] = (char)('0' + (value % 10));
		value /= 10;
	} while( value != 0 );
	return( std::string( buffer + pos ) );
}

} // namespace Text2yks
