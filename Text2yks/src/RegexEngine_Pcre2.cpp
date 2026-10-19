//---------------------------------------------------------------------------//
//  RegexEngine_Pcre2.cpp
//      RegexPattern の PCRE2 版
//      ・コンパイル: PCRE2_UTF | PCRE2_NEVER_BACKSLASH_C（\d \w \s は ASCII のみ。PCRE2_UCP は使わない）
//      ・照合: PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY（呼ぶ側が UTF-8 を検査済み・長さ0の一致は無視）
//      ・PCRE2_MATCH_INVALID_UTF は使わない（third_party\pcre2\README.Text2yks.md の「使い方の注意」）
//---------------------------------------------------------------------------//
#include "RegexEngine.h"
#include "Log.h"
#include "pcre2.h"

namespace Text2yks {

namespace {
const uint32_t MatchLimit = 1000000;   // 照合の回数の上限
const uint32_t DepthLimit = 10000;     // 照合の深さ（バックトラックの段数）の上限

//---------------------------------------------------------------------------//
//  std::string Pcre2ErrorMessage( int code )
//      引数 int code    PCRE2 のエラー番号
//      戻値 std::string エラーの説明（英語）
//      処理 PCRE2 のエラー番号を文字列にする
//---------------------------------------------------------------------------//
std::string Pcre2ErrorMessage( int code )
{
	PCRE2_UCHAR buffer[256];
	int length = pcre2_get_error_message( code, buffer, sizeof(buffer) );
	if( length < 0 ){
		return( std::string( "unknown PCRE2 error" ) );
	}
	return( std::string( (const char*)buffer, (size_t)length ) );
}
} // namespace

//---------------------------------------------------------------------------//
//  RegexPattern::RegexPattern()
//      引数 なし
//      戻値 なし
//      処理 空の状態で作る（Compile するまで使えない）
//---------------------------------------------------------------------------//
RegexPattern::RegexPattern()
	: Code( NULL ), MatchData( NULL ), MatchContext( NULL ), Captures( 0 )
{
}

//---------------------------------------------------------------------------//
//  RegexPattern::~RegexPattern()
//      引数 なし
//      戻値 なし
//      処理 PCRE2 の資源を解放する
//---------------------------------------------------------------------------//
RegexPattern::~RegexPattern()
{
	Release();
}

//---------------------------------------------------------------------------//
//  void RegexPattern::Release()
//      引数 なし
//      戻値 なし
//      処理 PCRE2 の資源を解放して空の状態へ戻す
//---------------------------------------------------------------------------//
void RegexPattern::Release()
{
	if( MatchData ){
		pcre2_match_data_free( (pcre2_match_data*)MatchData );
		MatchData = NULL;
	}
	if( MatchContext ){
		pcre2_match_context_free( (pcre2_match_context*)MatchContext );
		MatchContext = NULL;
	}
	if( Code ){
		pcre2_code_free( (pcre2_code*)Code );
		Code = NULL;
	}
	Captures = 0;
}

//---------------------------------------------------------------------------//
//  bool RegexPattern::Compile( const std::string& pattern, std::string& error )
//      引数 const std::string& pattern    パターン（UTF-8）
//           std::string&       error      失敗したときの理由（英語）
//      戻値 bool コンパイルできたら true
//      処理 パターンをコンパイルし、照合に使う領域と上限を用意する
//---------------------------------------------------------------------------//
bool RegexPattern::Compile( const std::string& pattern, std::string& error )
{
	Release();
	int        errorcode   = 0;
	PCRE2_SIZE erroroffset = 0;
	pcre2_code* code = pcre2_compile( (PCRE2_SPTR)pattern.data(), pattern.size(),
		PCRE2_UTF | PCRE2_NEVER_BACKSLASH_C, &errorcode, &erroroffset, NULL );
	if( code == NULL ){
		error = Pcre2ErrorMessage( errorcode ) + " (at byte " + NumberToString( (size_t)erroroffset ) + " of the pattern)";
		return( false );
	}
	uint32_t captures = 0;
	pcre2_pattern_info( code, PCRE2_INFO_CAPTURECOUNT, &captures );

	pcre2_match_data*    matchdata    = pcre2_match_data_create_from_pattern( code, NULL );
	pcre2_match_context* matchcontext = pcre2_match_context_create( NULL );
	if( matchdata == NULL || matchcontext == NULL ){
		if( matchdata )    pcre2_match_data_free( matchdata );
		if( matchcontext ) pcre2_match_context_free( matchcontext );
		pcre2_code_free( code );
		error = "out of memory";
		return( false );
	}
	pcre2_set_match_limit( matchcontext, MatchLimit );
	pcre2_set_depth_limit( matchcontext, DepthLimit );

	Code         = code;
	MatchData    = matchdata;
	MatchContext = matchcontext;
	Captures     = (int)captures;
	return( true );
}

//---------------------------------------------------------------------------//
//  int RegexPattern::CaptureCount() const
//      引数 なし
//      戻値 int 括弧のグループの数（一致全体は含まない）
//      処理 コンパイルしたパターンのグループ数を返す
//---------------------------------------------------------------------------//
int RegexPattern::CaptureCount() const
{
	return( Captures );
}

//---------------------------------------------------------------------------//
//  RegexPattern::FindResult RegexPattern::Find( const std::string& subject, size_t start, std::vector<MatchSpan>& groups, std::string& error )
//      引数 const std::string&      subject    対象（正しい UTF-8 であること。呼ぶ側で検査済み）
//           size_t                  start      探し始めるバイト位置（後読みはこれより前も見る）
//           std::vector<MatchSpan>& groups     一致したとき、[0]＝一致全体・[n]＝グループ n の範囲
//           std::string&            error      LIMIT_EXCEEDED・FAILED のときの理由（英語）
//      戻値 FindResult 一致したか
//      処理 start 以降で最も左の一致（長さ0は除く）を探す
//---------------------------------------------------------------------------//
RegexPattern::FindResult RegexPattern::Find( const std::string& subject, size_t start, std::vector<MatchSpan>& groups, std::string& error )
{
	if( Code == NULL ){
		error = "pattern is not compiled";
		return( FAILED );
	}
	int rc = pcre2_match( (pcre2_code*)Code, (PCRE2_SPTR)subject.data(), subject.size(), start,
		PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY, (pcre2_match_data*)MatchData, (pcre2_match_context*)MatchContext );
	if( rc == PCRE2_ERROR_NOMATCH ){
		return( NOT_FOUND );
	}
	if( rc == PCRE2_ERROR_MATCHLIMIT || rc == PCRE2_ERROR_DEPTHLIMIT || rc == PCRE2_ERROR_HEAPLIMIT || rc == PCRE2_ERROR_NOMEMORY ){
		error = Pcre2ErrorMessage( rc );
		return( LIMIT_EXCEEDED );
	}
	if( rc < 0 ){
		error = Pcre2ErrorMessage( rc );
		return( FAILED );
	}
	// rc == 0 は ovector が足りないときだけ（create_from_pattern なので起きない）
	PCRE2_SIZE* ovector = pcre2_get_ovector_pointer( (pcre2_match_data*)MatchData );
	groups.resize( (size_t)Captures + 1 );
	for( int i = 0; i <= Captures; i++ ){
		if( i < rc && ovector[i * 2] != PCRE2_UNSET ){
			groups[i].start = (size_t)ovector[i * 2];
			groups[i].end   = (size_t)ovector[i * 2 + 1];
		} else {
			groups[i].start = NoPosition;
			groups[i].end   = NoPosition;
		}
	}
	return( FOUND );
}

} // namespace Text2yks
