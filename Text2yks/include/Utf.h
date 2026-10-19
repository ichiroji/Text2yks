//---------------------------------------------------------------------------//
//  Utf.h
//      UTF-8 の検査・UTF-8 への変換（Windows API を使わない）
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_UTF_H
#define TEXT2YKS_UTF_H

#include <stddef.h>
#include <string>

namespace Text2yks {

// テキストファイルを UTF-8 へ読み替えた結果
enum DecodeResult {
	DECODE_OK = 0,          // 読めた
	DECODE_INVALID_UTF8,    // BOM が無い（または UTF-8 の BOM）が、正しい UTF-8 ではない
	DECODE_INVALID_UTF16    // UTF-16LE の BOM があるが、正しい UTF-16 ではない
};

bool         IsValidUtf8( const char* data, size_t length );
void         AppendUtf8( std::string& out, unsigned int code );
DecodeResult DecodeTextFile( const std::string& bytes, std::string& utf8 );

} // namespace Text2yks

#endif // TEXT2YKS_UTF_H
