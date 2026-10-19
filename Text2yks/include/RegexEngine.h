//---------------------------------------------------------------------------//
//  RegexEngine.h
//      正規表現エンジン（PCRE2）を包む薄い層
//      PCRE2 の型はここから外へ出さない。位置はすべて UTF-8 のバイト位置
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_REGEXENGINE_H
#define TEXT2YKS_REGEXENGINE_H

#include <stddef.h>
#include <string>
#include <vector>

namespace Text2yks {

// 一致した範囲（バイト位置）。値の無いグループは start・end とも NoPosition
struct MatchSpan {
	size_t start;
	size_t end;
};

class RegexPattern {
public:
	enum FindResult {
		FOUND = 0,          // 一致した
		NOT_FOUND,          // 一致しなかった
		LIMIT_EXCEEDED,     // 照合の上限（回数・深さ・メモリ）を超えた
		FAILED              // その他の失敗
	};
	static const size_t NoPosition = (size_t)-1;

	RegexPattern();
	~RegexPattern();

	bool        Compile( const std::string& pattern, std::string& error );
	int         CaptureCount() const;
	FindResult  Find( const std::string& subject, size_t start, std::vector<MatchSpan>& groups, std::string& error );

private:
	RegexPattern( const RegexPattern& );              // コピー禁止
	RegexPattern& operator=( const RegexPattern& );   // コピー禁止
	void Release();

	void* Code;            // pcre2_code_8*
	void* MatchData;       // pcre2_match_data_8*
	void* MatchContext;    // pcre2_match_context_8*
	int   Captures;
};

} // namespace Text2yks

#endif // TEXT2YKS_REGEXENGINE_H
