//---------------------------------------------------------------------------//
//  RuleFile.h
//      定義ファイル（Text2yks.txt）の読み込み
//      :include・:define・{NAME} の展開まで行い、規則の並び（パターンと置き換えの文字列）を作る
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_RULEFILE_H
#define TEXT2YKS_RULEFILE_H

#include <map>
#include <string>
#include <vector>
#include "FileSystem.h"

namespace Text2yks {

// 規則1つ分（正規表現のコンパイル前）
struct RuleSource {
	std::string pattern;        // {NAME} を展開した後のパターン
	std::string replacement;    // 置き換え（書式の解析前）
	std::string file;           // 書かれていたファイル
	int         line;           // 書かれていた行（1〜）
};

// 組込みの部品と予約名
extern const char* const BuiltinArgPattern;     // {ARG} の中身
extern const char* const ParenGroupName;        // {ARG} が使う括弧のグループ名（T2Y_PAREN）
extern const char* const ParenGroupDefinition;  // パターン末尾に足す (?(DEFINE)…)

class RuleFileReader {
public:
	explicit RuleFileReader( FileSystem& filesystem );

	bool Load( const std::string& path, std::vector<RuleSource>& rules, std::vector<std::string>& files );

private:
	RuleFileReader( const RuleFileReader& );              // コピー禁止
	RuleFileReader& operator=( const RuleFileReader& );   // コピー禁止

	bool LoadFile( const std::string& path, const std::string& fromfile, int fromline, int depth );
	void ParseLine( const std::string& line, const std::string& file, int lineno, int depth );
	bool Expand( const std::string& text, std::string& out, std::string& error ) const;

	FileSystem&                         Fs;
	std::map<std::string, std::string>  Defines;
	std::vector<std::string>            Stack;      // 読み込み中のファイル（循環の検出用・正規化した名前）
	std::vector<RuleSource>*            Rules;
	std::vector<std::string>*           Files;
};

} // namespace Text2yks

#endif // TEXT2YKS_RULEFILE_H
