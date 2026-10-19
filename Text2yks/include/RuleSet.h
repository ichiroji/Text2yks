//---------------------------------------------------------------------------//
//  RuleSet.h
//      規則の集まり（正規表現のコンパイル・置き換えの書式・行ごとの照合と周回）
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_RULESET_H
#define TEXT2YKS_RULESET_H

#include <stddef.h>
#include <string>
#include <vector>
#include "RegexEngine.h"
#include "RuleFile.h"

namespace Text2yks {

class RuleSet {
public:
	RuleSet();
	~RuleSet();

	void   Build( const std::vector<RuleSource>& sources );
	size_t RuleCount() const;
	bool   Apply( const char* text, size_t length, const std::string& filename, std::string& out );

private:
	RuleSet( const RuleSet& );              // コピー禁止
	RuleSet& operator=( const RuleSet& );   // コピー禁止

	// 置き換えの部品に付ける修飾子
	enum PieceModifier {
		PieceModifierNone  = 0,     // 値をそのまま入れる
		PieceModifierQuote = 1      // ${n:q}: 値の " を &#34; にして入れる
	};
	// 置き換えの部品（固定の文字列か、グループの番号）
	struct Piece {
		int         group;      // -1 なら literal、0 は一致全体、1〜 はグループ
		int         modifier;   // PieceModifier（literal のときは PieceModifierNone）
		std::string literal;
	};
	struct Rule {
		RegexPattern*      regex;
		std::string        prefix;     // 一致に必ず含まれる先頭の固定文字列（空なら絞り込まない）
		std::vector<Piece> pieces;
		std::string        file;
		int                line;
	};
	// 1周の中での、規則ごとの「次の一致」
	struct Candidate {
		size_t                 rule;
		int                    state;  // 0=未探索・1=一致あり・2=これ以上無い
		std::vector<MatchSpan> groups;
	};
	// Apply の1回分の状態
	struct ApplyContext {
		const std::string* filename;
		int                lineno;
		size_t             errors;      // この呼び出しで出したエラーの数
		size_t             suppressed;  // 上限を超えて出さなかったエラーの数
	};

	bool ApplyLine( const std::string& line, std::string& out, ApplyContext& context );
	bool OnePass( const std::string& line, std::string& out, ApplyContext& context );
	bool FindNext( const Rule& rule, const std::string& line, size_t start,
		const std::vector<size_t>& quotes, std::vector<MatchSpan>& groups, ApplyContext& context );
	void ReportError( ApplyContext& context, const std::string& message );

	std::vector<Rule> Rules;
};

} // namespace Text2yks

#endif // TEXT2YKS_RULESET_H
