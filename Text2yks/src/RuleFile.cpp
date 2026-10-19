//---------------------------------------------------------------------------//
//  RuleFile.cpp
//      定義ファイル（Text2yks.txt）の読み込み
//
//      1行の書き方（欄は TAB 1つ以上で区切る）
//        // コメント（行頭の空白のあとが //）・空行は無視
//        :include<TAB>"ファイル名"            読み込み元からの相対パス
//        :define<TAB>NAME<TAB>部分パターン    以後の {NAME} を部分パターンへ
//        パターン<TAB>置き換え[<TAB>//コメント]
//---------------------------------------------------------------------------//
#include "RuleFile.h"
#include <string.h>
#include "Log.h"
#include "Utf.h"

namespace Text2yks {

// {ARG}: 旧エイリアスの引数1個。前後の半角空白を含まない・空も可
//   部品 = 区切り(, ( ) " 半角空白)以外の並び | "…" | 釣り合った括弧
//   引数 = 部品 (半角空白* 部品)* を0か1回
const char* const BuiltinArgPattern =
	"(?:(?:[^,()\" ]++|\"[^\"]*+\"|(?&T2Y_PAREN))(?: *+(?:[^,()\" ]++|\"[^\"]*+\"|(?&T2Y_PAREN)))*+)?";
const char* const ParenGroupName = "T2Y_PAREN";
// 釣り合った括弧（中の "…" は数えない）。パターンの末尾に足すので利用者のグループ番号はずれない
const char* const ParenGroupDefinition =
	"(?(DEFINE)(?<T2Y_PAREN>\\((?:[^()\"]++|\"[^\"]*+\"|(?&T2Y_PAREN))*+\\)))";

namespace {
const int MaxIncludeDepth = 16;   // :include の入れ子の上限

//---------------------------------------------------------------------------//
//  void SplitFields( const std::string& line, std::vector<std::string>& fields )
//      引数 const std::string&        line      1行
//           std::vector<std::string>& fields    TAB の並びで区切った欄
//      戻値 なし
//      処理 行を TAB 1つ以上の並びで欄に分ける（行末の TAB は空の欄を作らない）
//---------------------------------------------------------------------------//
void SplitFields( const std::string& line, std::vector<std::string>& fields )
{
	fields.clear();
	size_t pos = 0;
	while( pos < line.size() ){
		size_t tab = line.find( '\t', pos );
		if( tab == std::string::npos ){
			fields.push_back( line.substr( pos ) );
			break;
		}
		fields.push_back( line.substr( pos, tab - pos ) );
		pos = line.find_first_not_of( '\t', tab );
		if( pos == std::string::npos ){
			break;
		}
	}
}

//---------------------------------------------------------------------------//
//  bool IsComment( const std::string& field )
//      引数 const std::string& field    欄
//      戻値 bool // で始まっていれば true
//      処理 欄がコメントかを調べる
//---------------------------------------------------------------------------//
bool IsComment( const std::string& field )
{
	return( field.size() >= 2 && field[0] == '/' && field[1] == '/' );
}

//---------------------------------------------------------------------------//
//  bool IsIdentifierStart( char c ) / bool IsIdentifierChar( char c )
//      引数 char c    文字
//      戻値 bool 名前の先頭（英字か _）・名前の2文字目以降（英数字か _）に使える文字なら true
//      処理 {NAME} の NAME に使える文字かを調べる
//---------------------------------------------------------------------------//
bool IsIdentifierStart( char c )
{
	return( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' );
}
bool IsIdentifierChar( char c )
{
	return( IsIdentifierStart( c ) || (c >= '0' && c <= '9') );
}

//---------------------------------------------------------------------------//
//  bool IsIdentifier( const std::string& name )
//      引数 const std::string& name    名前
//      戻値 bool {NAME} の NAME として正しければ true
//      処理 名前の形を調べる
//---------------------------------------------------------------------------//
bool IsIdentifier( const std::string& name )
{
	if( name.empty() || !IsIdentifierStart( name[0] ) ){
		return( false );
	}
	for( size_t i = 1; i < name.size(); i++ ){
		if( !IsIdentifierChar( name[i] ) ){
			return( false );
		}
	}
	return( true );
}

//---------------------------------------------------------------------------//
//  std::string NormalizePath( const std::string& path )
//      引数 const std::string& path    パス
//      戻値 std::string 比較用に正規化したパス
//      処理 / を \ に、ASCII の英字を小文字にする（循環の検出に使う）
//---------------------------------------------------------------------------//
std::string NormalizePath( const std::string& path )
{
	std::string out = path;
	for( size_t i = 0; i < out.size(); i++ ){
		if( out[i] == '/' ){
			out[i] = '\\';
		} else if( out[i] >= 'A' && out[i] <= 'Z' ){
			out[i] = (char)(out[i] - 'A' + 'a');
		}
	}
	return( out );
}

//---------------------------------------------------------------------------//
//  std::string ResolvePath( const std::string& fromfile, const std::string& target )
//      引数 const std::string& fromfile    読み込み元のファイル
//           const std::string& target      :include に書かれたパス
//      戻値 std::string 読むファイルのパス
//      処理 相対パスなら読み込み元のフォルダからのパスにする（絶対パスはそのまま）
//---------------------------------------------------------------------------//
std::string ResolvePath( const std::string& fromfile, const std::string& target )
{
	bool absolute = ( !target.empty() && (target[0] == '\\' || target[0] == '/') )
		|| ( target.size() >= 2 && target[1] == ':' );
	if( absolute ){
		return( target );
	}
	size_t slash = fromfile.find_last_of( "\\/" );
	if( slash == std::string::npos ){
		return( target );
	}
	return( fromfile.substr( 0, slash + 1 ) + target );
}

//---------------------------------------------------------------------------//
//  std::string Unquote( const std::string& text )
//      引数 const std::string& text    欄
//      戻値 std::string 前後の " を除いたもの（" で囲まれていなければそのまま）
//      処理 :include のファイル名の引用符を外す
//---------------------------------------------------------------------------//
std::string Unquote( const std::string& text )
{
	if( text.size() >= 2 && text[0] == '"' && text[text.size() - 1] == '"' ){
		return( text.substr( 1, text.size() - 2 ) );
	}
	return( text );
}
} // namespace

//---------------------------------------------------------------------------//
//  RuleFileReader::RuleFileReader( FileSystem& filesystem )
//      引数 FileSystem& filesystem    ファイルの読み込みに使うもの
//      戻値 なし
//      処理 読み込み器を作る
//---------------------------------------------------------------------------//
RuleFileReader::RuleFileReader( FileSystem& filesystem )
	: Fs( filesystem ), Rules( NULL ), Files( NULL )
{
}

//---------------------------------------------------------------------------//
//  bool RuleFileReader::Load( const std::string& path, std::vector<RuleSource>& rules, std::vector<std::string>& files )
//      引数 const std::string&        path     最初に読む定義ファイル（Text2yks.txt）
//           std::vector<RuleSource>&  rules    読んだ規則（書かれた順）
//           std::vector<std::string>& files    読もうとしたファイル（:include 先・見つからなかったものも含む）
//      戻値 bool 最初のファイルが読めたら true（中の誤りは飛ばして続けるので true になる）
//      処理 定義ファイルを読み、:include をたどって規則を並べる。誤りはログに出してその行を飛ばす
//---------------------------------------------------------------------------//
bool RuleFileReader::Load( const std::string& path, std::vector<RuleSource>& rules, std::vector<std::string>& files )
{
	rules.clear();
	files.clear();
	Defines.clear();
	Stack.clear();
	Rules = &rules;
	Files = &files;
	bool ok = LoadFile( path, std::string(), 0, 0 );
	Rules = NULL;
	Files = NULL;
	return( ok );
}

//---------------------------------------------------------------------------//
//  bool RuleFileReader::LoadFile( const std::string& path, const std::string& fromfile, int fromline, int depth )
//      引数 const std::string& path        読むファイル
//           const std::string& fromfile    読み込み元のファイル（最初のファイルなら空）
//           int                fromline    読み込み元の行
//           int                depth       :include の深さ（最初のファイルは0）
//      戻値 bool ファイルが読めたら true
//      処理 1つのファイルを読んで1行ずつ解析する。循環・深さの超過・読めない・文字コードの誤りはエラー
//---------------------------------------------------------------------------//
bool RuleFileReader::LoadFile( const std::string& path, const std::string& fromfile, int fromline, int depth )
{
	std::string where = fromfile.empty() ? std::string() : LogWhere( fromfile, fromline );
	std::string key = NormalizePath( path );
	for( size_t i = 0; i < Stack.size(); i++ ){
		if( Stack[i] == key ){
			LogError( where + "include cycle: \"" + path + "\"" );
			return( false );
		}
	}
	if( depth > MaxIncludeDepth ){
		LogError( where + "include nesting is too deep (more than " + NumberToString( MaxIncludeDepth ) + "): \"" + path + "\"" );
		return( false );
	}

	bool listed = false;
	for( size_t i = 0; i < Files->size(); i++ ){
		if( NormalizePath( (*Files)[i] ) == key ){
			listed = true;
			break;
		}
	}
	if( !listed ){
		Files->push_back( path );
	}

	std::string bytes;
	if( !Fs.ReadAll( path, bytes ) ){
		LogError( where + "cannot read definition file: \"" + path + "\"" );
		return( false );
	}
	std::string text;
	DecodeResult decoded = DecodeTextFile( bytes, text );
	if( decoded == DECODE_INVALID_UTF8 ){
		LogError( LogWhere( path, 0 ) + "file is not valid UTF-8 (save it as UTF-8 or UTF-16LE with BOM); the whole file is ignored" );
		return( false );
	}
	if( decoded == DECODE_INVALID_UTF16 ){
		LogError( LogWhere( path, 0 ) + "file is not valid UTF-16LE; the whole file is ignored" );
		return( false );
	}

	Stack.push_back( key );
	int lineno = 1;
	size_t pos = 0;
	while( pos <= text.size() ){
		size_t eol = text.find_first_of( "\r\n", pos );
		if( eol == std::string::npos ){
			if( pos < text.size() ){
				ParseLine( text.substr( pos ), path, lineno, depth );
			}
			break;
		}
		ParseLine( text.substr( pos, eol - pos ), path, lineno, depth );
		if( text[eol] == '\r' && eol + 1 < text.size() && text[eol + 1] == '\n' ){
			eol++;
		}
		pos = eol + 1;
		lineno++;
	}
	Stack.pop_back();
	return( true );
}

//---------------------------------------------------------------------------//
//  void RuleFileReader::ParseLine( const std::string& line, const std::string& file, int lineno, int depth )
//      引数 const std::string& line      1行（改行は含まない）
//           const std::string& file      ファイル名
//           int                lineno    行番号
//           int                depth     このファイルの :include の深さ
//      戻値 なし
//      処理 1行を解析して、命令なら実行し、規則なら Rules へ足す。誤りはログに出して飛ばす
//---------------------------------------------------------------------------//
void RuleFileReader::ParseLine( const std::string& line, const std::string& file, int lineno, int depth )
{
	std::string where = LogWhere( file, lineno );

	// 空行・コメント行
	size_t first = line.find_first_not_of( " \t" );
	if( first == std::string::npos ){
		return;
	}
	if( line.compare( first, 2, "//" ) == 0 ){
		return;
	}

	// 命令
	if( line[0] == ':' ){
		std::vector<std::string> fields;
		SplitFields( line, fields );
		const std::string& name = fields[0];
		if( name == ":include" ){
			if( fields.size() < 2 || fields.size() > 3 || (fields.size() == 3 && !IsComment( fields[2] )) ){
				LogError( where + ":include needs one file name (\":include<TAB>\"file\"\")" );
				return;
			}
			std::string target = Unquote( fields[1] );
			if( target.empty() ){
				LogError( where + ":include needs a file name" );
				return;
			}
			LoadFile( ResolvePath( file, target ), file, lineno, depth + 1 );
			return;
		}
		if( name == ":define" ){
			if( fields.size() < 3 || fields.size() > 4 || (fields.size() == 4 && !IsComment( fields[3] )) ){
				LogError( where + ":define needs a name and a pattern (\":define<TAB>NAME<TAB>pattern\")" );
				return;
			}
			const std::string& defname = fields[1];
			if( !IsIdentifier( defname ) ){
				LogError( where + "invalid name for :define: \"" + defname + "\" (use letters, digits and _)" );
				return;
			}
			if( defname == "ARG" || defname.compare( 0, 4, "T2Y_" ) == 0 ){
				LogError( where + "name is reserved: \"" + defname + "\"" );
				return;
			}
			std::string expanded;
			std::string error;
			if( !Expand( fields[2], expanded, error ) ){
				LogError( where + error );
				return;
			}
			if( Defines.find( defname ) != Defines.end() ){
				LogWarning( where + "{" + defname + "} is redefined" );
			}
			Defines[defname] = expanded;
			return;
		}
		LogError( where + "unknown directive: \"" + name + "\" (write \\: to start a pattern with ':')" );
		return;
	}

	// 規則: パターン<TAB>置き換え[<TAB>//コメント]
	size_t tab = line.find( '\t' );
	if( tab == std::string::npos ){
		LogError( where + "missing TAB between the pattern and the replacement" );
		return;
	}
	if( tab == 0 ){
		LogError( where + "empty pattern" );
		return;
	}
	RuleSource rule;
	rule.file = file;
	rule.line = lineno;
	std::string pattern = line.substr( 0, tab );
	size_t pos = line.find_first_not_of( '\t', tab );
	if( pos != std::string::npos ){
		size_t next = line.find( '\t', pos );
		rule.replacement = line.substr( pos, next == std::string::npos ? std::string::npos : next - pos );
		if( next != std::string::npos ){
			size_t rest = line.find_first_not_of( '\t', next );
			if( rest != std::string::npos && !IsComment( line.substr( rest ) ) ){
				LogError( where + "unexpected text after the replacement (a comment must start with //)" );
				return;
			}
		}
	}
	std::string error;
	if( !Expand( pattern, rule.pattern, error ) ){
		LogError( where + error );
		return;
	}
	Rules->push_back( rule );
}

//---------------------------------------------------------------------------//
//  bool RuleFileReader::Expand( const std::string& text, std::string& out, std::string& error ) const
//      引数 const std::string& text     パターン
//           std::string&       out      {NAME} を展開した結果
//           std::string&       error    失敗したときの理由（英語）
//      戻値 bool 展開できたら true（未定義の {NAME} があれば false）
//      処理 パターン中の {NAME} を :define の内容・組込みの部品へ置き換える
//           \ の直後の文字は展開しない（\{ はそのまま {）。{ の後が名前でなければ { はそのまま
//           \p{…} \P{…} \x{…} \o{…} \g{…} \k{…} \N{…} の {…} は PCRE2 の書き方なので展開しない
//---------------------------------------------------------------------------//
bool RuleFileReader::Expand( const std::string& text, std::string& out, std::string& error ) const
{
	out.clear();
	size_t i = 0;
	while( i < text.size() ){
		char c = text[i];
		if( c == '\\' ){
			out += c;
			if( i + 1 < text.size() ){
				char escape = text[i + 1];
				out += escape;
				i += 2;
				if( i < text.size() && text[i] == '{' && strchr( "pPxogkN", escape ) != NULL ){
					size_t close = text.find( '}', i );
					size_t end = ( close == std::string::npos ) ? text.size() : close + 1;
					out.append( text, i, end - i );
					i = end;
				}
			} else {
				i += 1;
			}
			continue;
		}
		if( c == '{' && i + 1 < text.size() && IsIdentifierStart( text[i + 1] ) ){
			size_t end = i + 1;
			while( end < text.size() && IsIdentifierChar( text[end] ) ){
				end++;
			}
			if( end < text.size() && text[end] == '}' ){
				std::string name = text.substr( i + 1, end - i - 1 );
				if( name == "ARG" ){
					out += BuiltinArgPattern;
				} else {
					std::map<std::string, std::string>::const_iterator it = Defines.find( name );
					if( it == Defines.end() ){
						error = "undefined name {" + name + "} (write \\{ for a literal '{')";
						return( false );
					}
					out += it->second;
				}
				i = end + 1;
				continue;
			}
		}
		out += c;
		i++;
	}
	return( true );
}

} // namespace Text2yks
