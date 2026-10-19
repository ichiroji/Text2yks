//---------------------------------------------------------------------------//
//  RegexProbe.cpp
//      同梱した PCRE2 が、このコンパイラ・このビット数で正しく動くかを確かめる確認用プログラム
//      結果を PASS/FAIL で表示し、FAIL の数を終了コードで返す（全部通れば 0）
//      VS2008 は UTF-8 の文字列リテラルを ANSI コードページへ変換してしまうため、
//      日本語などの文字はコードポイントから UTF-8 を組み立てて使う（ソースに直接書かない）
//---------------------------------------------------------------------------//
#include <stdio.h>
#include <string.h>
#include <string>
#include "pcre2.h"

namespace {

int FailCount = 0;

//---------------------------------------------------------------------------//
//  std::string Cp( unsigned int code )
//      引数 unsigned int code    Unicode のコードポイント
//      戻値 std::string その文字の UTF-8 バイト列
//      処理 コードポイント1つを UTF-8 へ変換する
//---------------------------------------------------------------------------//
std::string Cp( unsigned int code )
{
	std::string out;
	if( code < 0x80 ){
		out += (char)code;
	} else if( code < 0x800 ){
		out += (char)(0xC0 | (code >> 6));
		out += (char)(0x80 | (code & 0x3F));
	} else if( code < 0x10000 ){
		out += (char)(0xE0 | (code >> 12));
		out += (char)(0x80 | ((code >> 6) & 0x3F));
		out += (char)(0x80 | (code & 0x3F));
	} else {
		out += (char)(0xF0 | (code >> 18));
		out += (char)(0x80 | ((code >> 12) & 0x3F));
		out += (char)(0x80 | ((code >> 6) & 0x3F));
		out += (char)(0x80 | (code & 0x3F));
	}
	return( out );
}

//---------------------------------------------------------------------------//
//  void Report( const char* name, bool ok, const char* detail )
//      引数 const char* name      確認項目の名前
//           bool        ok        結果
//           const char* detail    失敗したときの説明（NULL 可）
//      戻値 なし
//      処理 確認結果を1行表示し、失敗なら FailCount を数える
//---------------------------------------------------------------------------//
void Report( const char* name, bool ok, const char* detail )
{
	if( ok ){
		printf( "PASS  %s\n", name );
	} else {
		printf( "FAIL  %s%s%s\n", name, detail ? " : " : "", detail ? detail : "" );
		FailCount++;
	}
}

//---------------------------------------------------------------------------//
//  pcre2_code* Compile( const std::string& pattern, uint32_t options )
//      引数 const std::string& pattern    パターン（UTF-8）
//           uint32_t           options    pcre2_compile のオプション
//      戻値 pcre2_code* コンパイル結果（失敗したら NULL。理由は表示する）
//      処理 パターンをコンパイルする
//---------------------------------------------------------------------------//
pcre2_code* Compile( const std::string& pattern, uint32_t options )
{
	int        errorcode   = 0;
	PCRE2_SIZE erroroffset = 0;
	pcre2_code* code = pcre2_compile( (PCRE2_SPTR)pattern.c_str(), pattern.size(), options, &errorcode, &erroroffset, NULL );
	if( code == NULL ){
		PCRE2_UCHAR message[256];
		pcre2_get_error_message( errorcode, message, sizeof(message) );
		printf( "      compile error %d at %u: %s\n", errorcode, (unsigned int)erroroffset, (const char*)message );
	}
	return( code );
}

//---------------------------------------------------------------------------//
//  int Match( const std::string& pattern, const std::string& subject, size_t* start, size_t* end, pcre2_match_context* mctx )
//      引数 const std::string&    pattern    パターン（UTF-8・PCRE2_UTF でコンパイル）
//           const std::string&    subject    対象の文字列（UTF-8）
//           size_t*               start      一致全体の開始バイト位置を返す先（NULL 可）
//           size_t*               end        一致全体の終了バイト位置を返す先（NULL 可）
//           pcre2_match_context*  mctx       照合の上限等（NULL なら既定）
//      戻値 int pcre2_match の戻り値（コンパイル失敗は -1000）
//      処理 パターンを1回照合する
//---------------------------------------------------------------------------//
int Match( const std::string& pattern, const std::string& subject, size_t* start, size_t* end, pcre2_match_context* mctx )
{
	pcre2_code* code = Compile( pattern, PCRE2_UTF );
	if( code == NULL ){
		return( -1000 );
	}
	pcre2_match_data* md = pcre2_match_data_create_from_pattern( code, NULL );
	int rc = pcre2_match( code, (PCRE2_SPTR)subject.c_str(), subject.size(), 0, 0, md, mctx );
	if( rc > 0 ){
		PCRE2_SIZE* ov = pcre2_get_ovector_pointer( md );
		if( start ) *start = ov[0];
		if( end )   *end   = ov[1];
	}
	pcre2_match_data_free( md );
	pcre2_code_free( code );
	return( rc );
}

//---------------------------------------------------------------------------//
//  bool MatchRange( const std::string& pattern, const std::string& subject, size_t start, size_t end )
//      引数 const std::string& pattern    パターン
//           const std::string& subject    対象の文字列
//           size_t             start      期待する開始バイト位置
//           size_t             end        期待する終了バイト位置
//      戻値 bool 一致し、位置が期待どおりなら true
//      処理 一致位置（UTF-8 のバイト位置）を確かめる
//---------------------------------------------------------------------------//
bool MatchRange( const std::string& pattern, const std::string& subject, size_t start, size_t end )
{
	size_t s = (size_t)-1;
	size_t e = (size_t)-1;
	int rc = Match( pattern, subject, &s, &e, NULL );
	if( rc <= 0 || s != start || e != end ){
		printf( "      rc=%d start=%d end=%d (expected %u-%u)\n", rc, (int)s, (int)e, (unsigned int)start, (unsigned int)end );
		return( false );
	}
	return( true );
}

//---------------------------------------------------------------------------//
//  void TestJapanese()
//      引数 なし
//      戻値 なし
//      処理 日本語の文で一致し、グループの位置が UTF-8 のバイト位置で返るか
//---------------------------------------------------------------------------//
void TestJapanese()
{
	// 台詞「こんにちは」です
	std::string hello = Cp(0x3053) + Cp(0x3093) + Cp(0x306B) + Cp(0x3061) + Cp(0x306F);
	std::string subject = Cp(0x53F0) + Cp(0x8A5E) + Cp(0x300C) + hello + Cp(0x300D) + Cp(0x3067) + Cp(0x3059);
	std::string pattern = Cp(0x300C) + "(.+?)" + Cp(0x300D);

	bool ok = false;
	pcre2_code* code = Compile( pattern, PCRE2_UTF );
	if( code ){
		pcre2_match_data* md = pcre2_match_data_create_from_pattern( code, NULL );
		int rc = pcre2_match( code, (PCRE2_SPTR)subject.c_str(), subject.size(), 0, 0, md, NULL );
		if( rc == 2 ){
			PCRE2_SIZE* ov = pcre2_get_ovector_pointer( md );
			ok = ( ov[0] == 6 && ov[1] == 27 && ov[2] == 9 && ov[3] == 24
				&& subject.compare( ov[2], ov[3] - ov[2], hello ) == 0 );
		}
		pcre2_match_data_free( md );
		pcre2_code_free( code );
	}
	Report( "japanese: group offsets are UTF-8 byte offsets", ok, NULL );

	// . 1つが日本語1文字（3バイト）に当たる
	Report( "japanese: '.' matches one 3-byte character", MatchRange( "^.$", Cp(0x6F22), 0, 3 ), NULL );
}

//---------------------------------------------------------------------------//
//  void TestOutsideBmp()
//      引数 なし
//      戻値 なし
//      処理 BMP 外の文字（4バイト）が1文字として扱われるか
//---------------------------------------------------------------------------//
void TestOutsideBmp()
{
	std::string yoshi = Cp(0x20BB7);   // 「土」の吉（U+20BB7）
	std::string smile = Cp(0x1F600);   // 絵文字の笑顔（U+1F600）
	Report( "outside BMP: '.' matches U+20BB7", MatchRange( "^.$", yoshi, 0, 4 ), NULL );
	Report( "outside BMP: '.' matches U+1F600", MatchRange( "^.$", smile, 0, 4 ), NULL );
	Report( "outside BMP: character class [U+20BB7 U+1F600]",
		MatchRange( "[" + yoshi + smile + "]+", "a" + yoshi + smile + "b", 1, 9 ), NULL );
}

//---------------------------------------------------------------------------//
//  void TestRecursion()
//      引数 なし
//      戻値 なし
//      処理 再帰で括弧の入れ子と "…" を数え、旧エイリアスの引数と同じ切り方ができるか（{ARG} 相当）
//---------------------------------------------------------------------------//
void TestRecursion()
{
	// 引数1個: 区切り文字以外・"…"・釣り合った括弧 の並び
	const char* define = "(?(DEFINE)(?<paren>\\((?:[^()\"]++|\"[^\"]*+\"|(?&paren))*\\)))";
	const char* arg    = "(?:[^,()\"]++|\"[^\"]*+\"|(?&paren))*+";
	std::string pattern = std::string( define ) + "Foo\\((?<a1>" + arg + "),(?<a2>" + arg + ")\\)";
	std::string subject = "x=Foo(a(b(c(d(e(f,g))))),\"x,(y\");";
	std::string want1   = "a(b(c(d(e(f,g)))))";
	std::string want2   = "\"x,(y\"";

	bool ok = false;
	pcre2_code* code = Compile( pattern, PCRE2_UTF );
	if( code ){
		pcre2_match_data* md = pcre2_match_data_create_from_pattern( code, NULL );
		int rc = pcre2_match( code, (PCRE2_SPTR)subject.c_str(), subject.size(), 0, 0, md, NULL );
		if( rc > 0 ){
			PCRE2_SIZE* ov = pcre2_get_ovector_pointer( md );
			int n1 = pcre2_substring_number_from_name( code, (PCRE2_SPTR)"a1" );
			int n2 = pcre2_substring_number_from_name( code, (PCRE2_SPTR)"a2" );
			if( n1 > 0 && n2 > 0 ){
				std::string got1 = subject.substr( ov[n1 * 2], ov[n1 * 2 + 1] - ov[n1 * 2] );
				std::string got2 = subject.substr( ov[n2 * 2], ov[n2 * 2 + 1] - ov[n2 * 2] );
				ok = ( got1 == want1 && got2 == want2 && ov[0] == 2 && ov[1] == subject.size() - 1 );
				if( !ok ){
					printf( "      a1=[%s] a2=[%s]\n", got1.c_str(), got2.c_str() );
				}
			}
		} else {
			printf( "      rc=%d\n", rc );
		}
		pcre2_match_data_free( md );
		pcre2_code_free( code );
	}
	Report( "recursion: nested parens (depth 5) and quoted ',' '(' split like {ARG}", ok, NULL );
}

//---------------------------------------------------------------------------//
//  void TestPerlFeatures()
//      引数 なし
//      戻値 なし
//      処理 Unicode の文字種・後読みが使えるか
//---------------------------------------------------------------------------//
void TestPerlFeatures()
{
	std::string kanji = Cp(0x6F22) + Cp(0x5B57);   // 漢字
	std::string hira  = Cp(0x3072) + Cp(0x3089);   // ひら
	std::string kata  = Cp(0x30AB);                // カ
	Report( "perl: \\p{Han}", MatchRange( "\\p{Han}+", "ab" + kanji + "cd", 2, 8 ), NULL );
	Report( "perl: \\p{Hiragana}", MatchRange( "\\p{Hiragana}+", kata + hira + kata, 3, 9 ), NULL );
	Report( "perl: lookbehind",
		MatchRange( "(?<=" + Cp(0x300C) + ")[^" + Cp(0x300D) + "]+", Cp(0x300C) + hira + Cp(0x300D), 3, 9 ), NULL );
}

//---------------------------------------------------------------------------//
//  void TestLimits()
//      引数 なし
//      戻値 なし
//      処理 照合の上限（回数・深さ）を超えたときに、止まらずエラーで返るか
//---------------------------------------------------------------------------//
void TestLimits()
{
	pcre2_match_context* mctx = pcre2_match_context_create( NULL );

	// 回数の上限: 破滅的なバックトラック
	pcre2_set_match_limit( mctx, 1000 );
	std::string many_a( 30, 'a' );
	int rc = Match( "(a+)+$", many_a + "b", NULL, NULL, mctx );
	Report( "limit: match limit returns PCRE2_ERROR_MATCHLIMIT", rc == PCRE2_ERROR_MATCHLIMIT, NULL );
	if( rc != PCRE2_ERROR_MATCHLIMIT ) printf( "      rc=%d\n", rc );

	// 深さの上限: 深い再帰
	pcre2_set_match_limit( mctx, 10000000 );
	pcre2_set_depth_limit( mctx, 50 );
	std::string nested = std::string( 200, '(' ) + std::string( 200, ')' );
	rc = Match( "^(?<p>\\((?&p)*\\))$", nested, NULL, NULL, mctx );
	Report( "limit: depth limit returns PCRE2_ERROR_DEPTHLIMIT", rc == PCRE2_ERROR_DEPTHLIMIT, NULL );
	if( rc != PCRE2_ERROR_DEPTHLIMIT ) printf( "      rc=%d\n", rc );

	// 上限内なら同じ入れ子が一致する
	pcre2_set_depth_limit( mctx, 10000 );
	rc = Match( "^(?<p>\\((?&p)*\\))$", nested, NULL, NULL, mctx );
	Report( "limit: same nesting matches within the limit", rc > 0, NULL );
	if( rc <= 0 ) printf( "      rc=%d\n", rc );

	pcre2_match_context_free( mctx );
}

//---------------------------------------------------------------------------//
//  void TestInvalidUtf8()
//      引数 なし
//      戻値 なし
//      処理 PCRE2_MATCH_INVALID_UTF を使わない場合、不正な UTF-8 は範囲外を読まずに
//           PCRE2_ERROR_UTF8_* で拒否されるか（自前検査の取りこぼしがあっても安全側に倒れること）
//---------------------------------------------------------------------------//
void TestInvalidUtf8()
{
	const char* names[3] = { "invalid UTF-8: stray 0xFF", "invalid UTF-8: overlong 0xC0 0xAF", "invalid UTF-8: truncated sequence at end" };
	std::string subjects[3];
	subjects[0] = std::string( "abc" ) + (char)0xFF + "def";
	subjects[1] = std::string( "abc" ) + (char)0xC0 + (char)0xAF + "def";
	subjects[2] = std::string( "abcdef" ) + (char)0xE3 + (char)0x81;
	for( int i = 0; i < 3; i++ ){
		int rc = Match( "def", subjects[i], NULL, NULL, NULL );
		bool ok = ( rc <= PCRE2_ERROR_UTF8_ERR1 && rc >= PCRE2_ERROR_UTF8_ERR21 );
		Report( names[i], ok, NULL );
		if( !ok ) printf( "      rc=%d\n", rc );
	}
}

//---------------------------------------------------------------------------//
//  void TestConfig()
//      引数 なし
//      戻値 なし
//      処理 組み込んだ PCRE2 の版と構成（JIT 無し・Unicode 有り）を確かめる
//---------------------------------------------------------------------------//
void TestConfig()
{
	char version[64];
	memset( version, 0, sizeof(version) );
	pcre2_config( PCRE2_CONFIG_VERSION, version );
	printf( "      PCRE2 version: %s\n", version );
	Report( "config: version is 10.39", strncmp( version, "10.39 ", 6 ) == 0, NULL );

	uint32_t jit = 1;
	pcre2_config( PCRE2_CONFIG_JIT, &jit );
	Report( "config: JIT is disabled", jit == 0, NULL );

	uint32_t unicode = 0;
	pcre2_config( PCRE2_CONFIG_UNICODE, &unicode );
	Report( "config: Unicode is enabled", unicode == 1, NULL );
}

} // namespace

//---------------------------------------------------------------------------//
//  int main()
//      引数 なし
//      戻値 int FAIL の数（0 なら全部通った）
//      処理 確認項目を順に実行して結果を表示する
//---------------------------------------------------------------------------//
int main()
{
	printf( "RegexProbe  _MSC_VER=%d  %d-bit\n", (int)_MSC_VER, (int)(sizeof(void*) * 8) );
	TestConfig();
	TestJapanese();
	TestOutsideBmp();
	TestRecursion();
	TestPerlFeatures();
	TestLimits();
	TestInvalidUtf8();
	printf( "%s (%d failed)\n", FailCount == 0 ? "ALL PASSED" : "FAILED", FailCount );
	return( FailCount );
}
