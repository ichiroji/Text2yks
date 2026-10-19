//---------------------------------------------------------------------------//
//  RuleSet.cpp
//      規則の集まり（正規表現のコンパイル・置き換えの書式・行ごとの照合と周回）
//
//      置き換えの動き
//        ・テキストを \r\n・\n・\r で行に分け、行の中だけで照合・置換する（行数は変わらない）
//        ・1周: 行の左から、全規則の中で最も左で始まる一致（同じ位置なら定義が先の規則）を置き換える
//               "…" の区間 [開く " , 閉じる " ) に始まる一致は当てない（閉じない " は行末まで）
//               置き換えた部分はその周では見直さない
//        ・行が変わった間は周回を続ける（上限 MaxPasses 周）
//        ・不正な UTF-8 の行は何もしない
//---------------------------------------------------------------------------//
#include "RuleSet.h"
#include <string.h>
#include "Log.h"
#include "Utf.h"

namespace Text2yks {

namespace {
const int    MaxPasses       = 32;   // 1行の周回の上限
const size_t MaxErrorsPerCall = 20;  // 1回の Apply で出すエラーの上限

//---------------------------------------------------------------------------//
//  size_t Utf8SequenceLength( unsigned char lead )
//      引数 unsigned char lead    UTF-8 の先頭のバイト
//      戻値 size_t その文字のバイト数（1〜4。続きのバイトなら1）
//      処理 UTF-8 の1文字の長さを先頭のバイトから求める
//---------------------------------------------------------------------------//
size_t Utf8SequenceLength( unsigned char lead )
{
	if( lead >= 0xF0 ) return( 4 );
	if( lead >= 0xE0 ) return( 3 );
	if( lead >= 0xC0 ) return( 2 );
	return( 1 );
}

//---------------------------------------------------------------------------//
//  bool IsAsciiAlnum( char c )
//      引数 char c    文字
//      戻値 bool ASCII の英数字なら true
//      処理 \ の後が文字種・番号などの特別な意味を持つかを調べるのに使う
//---------------------------------------------------------------------------//
bool IsAsciiAlnum( char c )
{
	return( (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') );
}

//---------------------------------------------------------------------------//
//  bool HasLimitVerb( const std::string& pattern )
//      引数 const std::string& pattern    パターン
//      戻値 bool 先頭に (*LIMIT_…) があれば true
//      処理 パターン先頭の (*…) の並びに上限の指定があるかを調べる（PCRE2 10.39 の既知の不具合を避けるため禁止）
//---------------------------------------------------------------------------//
bool HasLimitVerb( const std::string& pattern )
{
	size_t i = 0;
	while( pattern.compare( i, 2, "(*" ) == 0 ){
		size_t close = pattern.find( ')', i );
		if( close == std::string::npos ){
			break;
		}
		if( pattern.compare( i + 2, 6, "LIMIT_" ) == 0 ){
			return( true );
		}
		i = close + 1;
	}
	return( false );
}

//---------------------------------------------------------------------------//
//  bool UsesReservedGroupName( const std::string& pattern )
//      引数 const std::string& pattern    パターン（組込みの定義を足す前）
//      戻値 bool T2Y_ で始まる名前のグループを定義していれば true
//      処理 予約名のグループを利用者が作っていないかを調べる
//---------------------------------------------------------------------------//
bool UsesReservedGroupName( const std::string& pattern )
{
	return( pattern.find( "(?<T2Y_" ) != std::string::npos
		|| pattern.find( "(?'T2Y_" ) != std::string::npos
		|| pattern.find( "(?P<T2Y_" ) != std::string::npos );
}

//---------------------------------------------------------------------------//
//  bool MayHaveTopLevelAlternation( const std::string& pattern )
//      引数 const std::string& pattern    パターン
//      戻値 bool 括弧の外に | があるかもしれなければ true
//      処理 先頭の固定文字列で絞り込んでよいかを調べる（判断できない書き方は「あるかもしれない」とする）
//           \Q…\E・[:…:]・(?#…) のコメント・(?x) の拡張書式（# から後がコメント）は括弧を正しく数えられないので「ある」とする
//---------------------------------------------------------------------------//
bool MayHaveTopLevelAlternation( const std::string& pattern )
{
	if( pattern.find( "\\Q" ) != std::string::npos || pattern.find( "[:" ) != std::string::npos
		|| pattern.find( "(?#" ) != std::string::npos ){
		return( true );
	}
	// (?x)・(?ix:…) などの拡張書式の指定
	for( size_t open = pattern.find( "(?" ); open != std::string::npos; open = pattern.find( "(?", open + 2 ) ){
		for( size_t k = open + 2; k < pattern.size(); k++ ){
			char c = pattern[k];
			if( c == 'x' ){
				return( true );
			}
			if( !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '^' || c == '-') ){
				break;
			}
		}
	}
	int depth = 0;
	size_t i = 0;
	while( i < pattern.size() ){
		char c = pattern[i];
		if( c == '\\' ){
			i += 2;
			continue;
		}
		if( c == '[' ){
			// 文字クラスの終わりまで飛ばす（先頭の ^ と ] は文字）
			i++;
			if( i < pattern.size() && pattern[i] == '^' ) i++;
			if( i < pattern.size() && pattern[i] == ']' ) i++;
			while( i < pattern.size() && pattern[i] != ']' ){
				i += ( pattern[i] == '\\' ) ? 2 : 1;
			}
			i++;
			continue;
		}
		if( c == '(' ){
			depth++;
		} else if( c == ')' ){
			depth--;
		} else if( c == '|' && depth <= 0 ){
			return( true );
		}
		i++;
	}
	return( false );
}

//---------------------------------------------------------------------------//
//  std::string ExtractPrefix( const std::string& pattern )
//      引数 const std::string& pattern    パターン（{NAME} 展開後・組込みの定義を足す前）
//      戻値 std::string 一致に必ず含まれる先頭の固定文字列（UTF-8。分からなければ空）
//      処理 行の絞り込みに使う固定文字列を取り出す。先頭の ^ は飛ばす
//           特別な文字・\英数字・数の指定が付く文字の手前で止める
//---------------------------------------------------------------------------//
std::string ExtractPrefix( const std::string& pattern )
{
	std::string prefix;
	if( MayHaveTopLevelAlternation( pattern ) ){
		return( prefix );
	}
	size_t i = 0;
	if( !pattern.empty() && pattern[0] == '^' ){
		i = 1;
	}
	while( i < pattern.size() ){
		char c = pattern[i];
		std::string unit;
		if( c == '\\' ){
			if( i + 1 >= pattern.size() || IsAsciiAlnum( pattern[i + 1] ) ){
				break;
			}
			size_t length = Utf8SequenceLength( (unsigned char)pattern[i + 1] );
			unit = pattern.substr( i + 1, length );
			i += 1 + length;
		} else if( strchr( ".^$|?*+()[]{}", c ) != NULL ){
			break;
		} else {
			size_t length = Utf8SequenceLength( (unsigned char)c );
			unit = pattern.substr( i, length );
			i += length;
		}
		if( i < pattern.size() && strchr( "?*+{", pattern[i] ) != NULL ){
			break;
		}
		prefix += unit;
	}
	return( prefix );
}

//---------------------------------------------------------------------------//
//  void FindQuotes( const std::string& line, std::vector<size_t>& quotes )
//      引数 const std::string&   line      1行
//           std::vector<size_t>& quotes    "…" の区間 [開く位置, 閉じる位置) を2つずつ並べたもの
//      戻値 なし
//      処理 行の中の "…" の区間を求める。閉じない " は行末までの区間にする（旧版と同じ）
//---------------------------------------------------------------------------//
void FindQuotes( const std::string& line, std::vector<size_t>& quotes )
{
	quotes.clear();
	size_t pos = line.find( '"' );
	while( pos != std::string::npos ){
		size_t close = line.find( '"', pos + 1 );
		quotes.push_back( pos );
		if( close == std::string::npos ){
			quotes.push_back( line.size() );
			break;
		}
		quotes.push_back( close );
		pos = line.find( '"', close + 1 );
	}
}

//---------------------------------------------------------------------------//
//  size_t QuoteEnd( const std::vector<size_t>& quotes, size_t pos )
//      引数 const std::vector<size_t>& quotes    FindQuotes で求めた区間
//           size_t                     pos       調べる位置
//      戻値 size_t pos が "…" の区間にあればその区間の終わり（閉じる " の位置）、無ければ npos
//      処理 一致の開始位置が "…" の中かを調べる
//---------------------------------------------------------------------------//
size_t QuoteEnd( const std::vector<size_t>& quotes, size_t pos )
{
	for( size_t i = 0; i + 1 < quotes.size(); i += 2 ){
		if( pos < quotes[i] ){
			break;
		}
		if( pos < quotes[i + 1] ){
			return( quotes[i + 1] );
		}
	}
	return( std::string::npos );
}
} // namespace

//---------------------------------------------------------------------------//
//  RuleSet::RuleSet()
//      引数 なし
//      戻値 なし
//      処理 規則の無い状態で作る
//---------------------------------------------------------------------------//
RuleSet::RuleSet()
{
}

//---------------------------------------------------------------------------//
//  RuleSet::~RuleSet()
//      引数 なし
//      戻値 なし
//      処理 コンパイルした正規表現を解放する
//---------------------------------------------------------------------------//
RuleSet::~RuleSet()
{
	for( size_t i = 0; i < Rules.size(); i++ ){
		delete Rules[i].regex;
	}
}

//---------------------------------------------------------------------------//
//  void RuleSet::Build( const std::vector<RuleSource>& sources )
//      引数 const std::vector<RuleSource>& sources    定義ファイルから読んだ規則
//      戻値 なし
//      処理 規則をコンパイルし、置き換えの書式を解析し、絞り込み用の固定文字列を取り出す
//           誤りのある規則はログに出して飛ばす
//---------------------------------------------------------------------------//
void RuleSet::Build( const std::vector<RuleSource>& sources )
{
	for( size_t i = 0; i < Rules.size(); i++ ){
		delete Rules[i].regex;
	}
	Rules.clear();

	for( size_t s = 0; s < sources.size(); s++ ){
		const RuleSource& source = sources[s];
		std::string where = LogWhere( source.file, source.line );

		if( HasLimitVerb( source.pattern ) ){
			LogError( where + "(*LIMIT_...) at the start of a pattern is not allowed" );
			continue;
		}
		if( UsesReservedGroupName( source.pattern ) ){
			LogError( where + "group names starting with T2Y_ are reserved" );
			continue;
		}

		// {ARG} を使っていれば括弧の定義を末尾に足す（利用者のグループ番号はずれない）
		bool usesparen = ( source.pattern.find( std::string( "(?&" ) + ParenGroupName + ")" ) != std::string::npos );
		std::string fullpattern = source.pattern;
		if( usesparen ){
			fullpattern += ParenGroupDefinition;
		}

		Rule rule;
		rule.regex = new RegexPattern();
		rule.file  = source.file;
		rule.line  = source.line;
		std::string error;
		if( !rule.regex->Compile( fullpattern, error ) ){
			LogError( where + "invalid pattern: " + error );
			delete rule.regex;
			continue;
		}
		int usergroups = rule.regex->CaptureCount() - ( usesparen ? 1 : 0 );

		// 置き換えの書式: $1〜$99（2桁はそのグループがあるときだけ）・$&・$$。それ以外はそのまま
		bool ok = true;
		std::string literal;
		const std::string& text = source.replacement;
		size_t i = 0;
		while( i < text.size() ){
			char c = text[i];
			if( c != '$' || i + 1 >= text.size() ){
				literal += c;
				i++;
				continue;
			}
			char d = text[i + 1];
			int  group = -1;
			size_t length = 2;
			if( d == '$' ){
				literal += '$';
				i += 2;
				continue;
			}
			if( d == '&' ){
				group = 0;
			} else if( d >= '1' && d <= '9' ){
				group = d - '0';
				if( i + 2 < text.size() && text[i + 2] >= '0' && text[i + 2] <= '9' ){
					int twodigits = group * 10 + ( text[i + 2] - '0' );
					if( twodigits <= usergroups ){
						group  = twodigits;
						length = 3;
					}
				}
				if( group > usergroups ){
					LogError( where + "replacement refers to $" + NumberToString( (size_t)group )
						+ " but the pattern has only " + NumberToString( (size_t)usergroups ) + " group(s)" );
					ok = false;
					break;
				}
			} else {
				literal += c;
				i++;
				continue;
			}
			if( !literal.empty() ){
				Piece piece;
				piece.group   = -1;
				piece.literal = literal;
				rule.pieces.push_back( piece );
				literal.clear();
			}
			Piece piece;
			piece.group = group;
			rule.pieces.push_back( piece );
			i += length;
		}
		if( !ok ){
			delete rule.regex;
			continue;
		}
		if( !literal.empty() ){
			Piece piece;
			piece.group   = -1;
			piece.literal = literal;
			rule.pieces.push_back( piece );
		}

		rule.prefix = ExtractPrefix( source.pattern );
		Rules.push_back( rule );
	}
}

//---------------------------------------------------------------------------//
//  size_t RuleSet::RuleCount() const
//      引数 なし
//      戻値 size_t 使える規則の数
//      処理 Build で残った規則の数を返す
//---------------------------------------------------------------------------//
size_t RuleSet::RuleCount() const
{
	return( Rules.size() );
}

//---------------------------------------------------------------------------//
//  bool RuleSet::Apply( const char* text, size_t length, const std::string& filename, std::string& out )
//      引数 const char*        text        スクリプトの文字列（UTF-8）
//           size_t             length      バイト数
//           const std::string& filename    スクリプトの名前（ログ用）
//           std::string&       out         置き換えた結果（戻値が true のときだけ意味がある）
//      戻値 bool どこかの行が変わったら true
//      処理 テキストを行に分けて規則を当てる。改行コードは元のまま書き戻す
//---------------------------------------------------------------------------//
bool RuleSet::Apply( const char* text, size_t length, const std::string& filename, std::string& out )
{
	out.clear();
	if( Rules.empty() ){
		return( false );
	}
	ApplyContext context;
	context.filename   = &filename;
	context.lineno     = 0;
	context.errors     = 0;
	context.suppressed = 0;

	out.reserve( length + length / 8 );
	bool changed = false;
	std::string line;
	std::string replaced;
	size_t pos = 0;
	int lineno = 1;
	while( pos < length ){
		size_t eol = pos;
		while( eol < length && text[eol] != '\r' && text[eol] != '\n' ){
			eol++;
		}
		line.assign( text + pos, eol - pos );
		context.lineno = lineno;
		if( ApplyLine( line, replaced, context ) ){
			out += replaced;
			changed = true;
		} else {
			out += line;
		}
		if( eol < length ){
			if( text[eol] == '\r' && eol + 1 < length && text[eol + 1] == '\n' ){
				out += "\r\n";
				eol += 2;
			} else {
				out += text[eol];
				eol += 1;
			}
		}
		pos = eol;
		lineno++;
	}
	if( context.suppressed > 0 ){
		LogError( LogWhere( filename, 0 ) + NumberToString( context.suppressed ) + " more error(s) were not shown" );
	}
	return( changed );
}

//---------------------------------------------------------------------------//
//  bool RuleSet::ApplyLine( const std::string& line, std::string& out, ApplyContext& context )
//      引数 const std::string& line       1行（改行は含まない）
//           std::string&       out        置き換えた結果（戻値が true のときだけ意味がある）
//           ApplyContext&      context    呼び出し1回分の状態
//      戻値 bool 行が変わったら true
//      処理 行が変わらなくなるまで周回する。MaxPasses 周しても変わり続けるならエラーを出し、その時点の結果にする
//---------------------------------------------------------------------------//
bool RuleSet::ApplyLine( const std::string& line, std::string& out, ApplyContext& context )
{
	if( !IsValidUtf8( line.data(), line.size() ) ){
		return( false );
	}
	std::string current;
	std::string next;
	bool changed = false;
	for( int pass = 0; pass < MaxPasses; pass++ ){
		if( !OnePass( changed ? current : line, next, context ) ){
			break;
		}
		current.swap( next );
		changed = true;
		if( pass + 1 == MaxPasses ){
			if( OnePass( current, next, context ) ){
				ReportError( context, LogWhere( *context.filename, context.lineno )
					+ "replacement did not settle after " + NumberToString( MaxPasses )
					+ " passes (rules may be replacing each other in a cycle); the result of the last pass is used" );
			}
		}
	}
	if( changed ){
		out.swap( current );
	}
	return( changed );
}

//---------------------------------------------------------------------------//
//  bool RuleSet::OnePass( const std::string& line, std::string& out, ApplyContext& context )
//      引数 const std::string& line       1行
//           std::string&       out        この周の結果
//           ApplyContext&      context    呼び出し1回分の状態
//      戻値 bool 行が変わったら true（同じ文字列への置き換えは変わったとみなさない）
//      処理 行の左から、最も左で始まる一致（同じ位置なら定義が先の規則）を順に置き換える
//           各規則の「次の一致」を覚えておき、使った範囲と重なったものだけ探し直す
//---------------------------------------------------------------------------//
bool RuleSet::OnePass( const std::string& line, std::string& out, ApplyContext& context )
{
	std::vector<Candidate> candidates;
	for( size_t r = 0; r < Rules.size(); r++ ){
		if( Rules[r].prefix.empty() || line.find( Rules[r].prefix ) != std::string::npos ){
			Candidate candidate;
			candidate.rule  = r;
			candidate.state = 0;
			candidates.push_back( candidate );
		}
	}
	if( candidates.empty() ){
		return( false );
	}

	std::vector<size_t> quotes;
	FindQuotes( line, quotes );

	out.clear();
	bool changed = false;
	size_t pos = 0;
	for( ;; ){
		int best = -1;
		for( size_t i = 0; i < candidates.size(); i++ ){
			Candidate& candidate = candidates[i];
			if( candidate.state == 2 ){
				continue;
			}
			if( candidate.state == 1 && candidate.groups[0].start < pos ){
				candidate.state = 0;
			}
			if( candidate.state == 0 ){
				candidate.state = FindNext( Rules[candidate.rule], line, pos, quotes, candidate.groups, context ) ? 1 : 2;
			}
			if( candidate.state == 1 && (best < 0 || candidate.groups[0].start < candidates[best].groups[0].start) ){
				best = (int)i;
			}
		}
		if( best < 0 ){
			break;
		}

		Candidate& chosen = candidates[best];
		const Rule& rule  = Rules[chosen.rule];
		size_t matchstart = chosen.groups[0].start;
		size_t matchend   = chosen.groups[0].end;
		out.append( line, pos, matchstart - pos );
		size_t before = out.size();
		for( size_t p = 0; p < rule.pieces.size(); p++ ){
			const Piece& piece = rule.pieces[p];
			if( piece.group < 0 ){
				out += piece.literal;
			} else {
				const MatchSpan& span = chosen.groups[piece.group];
				if( span.start != RegexPattern::NoPosition ){
					out.append( line, span.start, span.end - span.start );
				}
			}
		}
		if( out.size() - before != matchend - matchstart || out.compare( before, std::string::npos, line, matchstart, matchend - matchstart ) != 0 ){
			changed = true;
		}
		pos = matchend;
		chosen.state = 0;
	}
	out.append( line, pos, std::string::npos );
	return( changed );
}

//---------------------------------------------------------------------------//
//  bool RuleSet::FindNext( const Rule& rule, const std::string& line, size_t start,
//                          const std::vector<size_t>& quotes, std::vector<MatchSpan>& groups, ApplyContext& context )
//      引数 const Rule&                rule       規則
//           const std::string&         line       1行
//           size_t                     start      探し始める位置
//           const std::vector<size_t>& quotes     "…" の区間
//           std::vector<MatchSpan>&    groups     見つかった一致
//           ApplyContext&              context    呼び出し1回分の状態
//      戻値 bool 当てられる一致が見つかったら true
//      処理 start 以降で、"…" の区間に始まらない最初の一致を探す。照合の上限を超えたらエラーを出して見つからない扱い
//---------------------------------------------------------------------------//
bool RuleSet::FindNext( const Rule& rule, const std::string& line, size_t start,
	const std::vector<size_t>& quotes, std::vector<MatchSpan>& groups, ApplyContext& context )
{
	while( start <= line.size() ){
		std::string error;
		RegexPattern::FindResult result = rule.regex->Find( line, start, groups, error );
		if( result == RegexPattern::NOT_FOUND ){
			return( false );
		}
		if( result != RegexPattern::FOUND ){
			ReportError( context, LogWhere( *context.filename, context.lineno ) + "matching stopped (" + error
				+ ") for the rule at " + LogWhere( rule.file, rule.line ) + "the rule is not applied to this line" );
			return( false );
		}
		size_t quoteend = QuoteEnd( quotes, groups[0].start );
		if( quoteend == std::string::npos ){
			return( true );
		}
		start = quoteend;
	}
	return( false );
}

//---------------------------------------------------------------------------//
//  void RuleSet::ReportError( ApplyContext& context, const std::string& message )
//      引数 ApplyContext&      context    呼び出し1回分の状態
//           const std::string& message    内容
//      戻値 なし
//      処理 エラーを出す。1回の呼び出しで MaxErrorsPerCall 件を超えたら数えるだけにする
//---------------------------------------------------------------------------//
void RuleSet::ReportError( ApplyContext& context, const std::string& message )
{
	if( context.errors < MaxErrorsPerCall ){
		LogError( message );
		context.errors++;
	} else {
		context.suppressed++;
	}
}

} // namespace Text2yks
