//---------------------------------------------------------------------------//
//  Utf.cpp
//      UTF-8 の検査・UTF-8 への変換（Windows API を使わない）
//---------------------------------------------------------------------------//
#include "Utf.h"

namespace Text2yks {

//---------------------------------------------------------------------------//
//  bool IsValidUtf8( const char* data, size_t length )
//      引数 const char* data      検査するバイト列
//           size_t      length    バイト数
//      戻値 bool 正しい UTF-8 なら true
//      処理 バイト列が正しい UTF-8 かを調べる
//           冗長な表現・サロゲートの範囲（U+D800〜U+DFFF）・U+10FFFF を超えるもの・途中で切れたものは不正とする
//---------------------------------------------------------------------------//
bool IsValidUtf8( const char* data, size_t length )
{
	const unsigned char* p = (const unsigned char*)data;
	size_t i = 0;
	while( i < length ){
		unsigned char b = p[i];
		if( b <= 0x7F ){
			i += 1;
			continue;
		}
		size_t        count = 0;     // 続くバイトの数
		unsigned char lower = 0x80;  // 2バイト目の下限
		unsigned char upper = 0xBF;  // 2バイト目の上限
		if( b >= 0xC2 && b <= 0xDF ){
			count = 1;
		} else if( b == 0xE0 ){
			count = 2; lower = 0xA0;
		} else if( b >= 0xE1 && b <= 0xEC ){
			count = 2;
		} else if( b == 0xED ){
			count = 2; upper = 0x9F;
		} else if( b >= 0xEE && b <= 0xEF ){
			count = 2;
		} else if( b == 0xF0 ){
			count = 3; lower = 0x90;
		} else if( b >= 0xF1 && b <= 0xF3 ){
			count = 3;
		} else if( b == 0xF4 ){
			count = 3; upper = 0x8F;
		} else {
			return( false );
		}
		if( length - i <= count ){
			return( false );
		}
		if( p[i + 1] < lower || p[i + 1] > upper ){
			return( false );
		}
		for( size_t k = 2; k <= count; k++ ){
			if( (p[i + k] & 0xC0) != 0x80 ){
				return( false );
			}
		}
		i += count + 1;
	}
	return( true );
}

//---------------------------------------------------------------------------//
//  void AppendUtf8( std::string& out, unsigned int code )
//      引数 std::string& out     追加先
//           unsigned int code    Unicode のコードポイント（U+10FFFF 以下・サロゲート以外）
//      戻値 なし
//      処理 コードポイント1つを UTF-8 にして out の末尾へ足す
//---------------------------------------------------------------------------//
void AppendUtf8( std::string& out, unsigned int code )
{
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
}

//---------------------------------------------------------------------------//
//  DecodeResult DecodeTextFile( const std::string& bytes, std::string& utf8 )
//      引数 const std::string& bytes    ファイルの中身
//           std::string&       utf8     UTF-8 にした結果（BOM は除く）
//      戻値 DecodeResult 読めたか・読めなかった理由
//      処理 定義ファイルの中身を UTF-8 にする
//           UTF-8 の BOM 付き・BOM 無しの UTF-8・UTF-16LE の BOM 付きを読む。CP932 などは読まない
//---------------------------------------------------------------------------//
DecodeResult DecodeTextFile( const std::string& bytes, std::string& utf8 )
{
	utf8.clear();
	const unsigned char* p = (const unsigned char*)bytes.data();
	size_t length = bytes.size();

	// UTF-16LE（BOM 必須）
	if( length >= 2 && p[0] == 0xFF && p[1] == 0xFE ){
		if( (length % 2) != 0 ){
			return( DECODE_INVALID_UTF16 );
		}
		size_t i = 2;
		while( i < length ){
			unsigned int unit = p[i] | (p[i + 1] << 8);
			i += 2;
			if( unit >= 0xD800 && unit <= 0xDBFF ){
				if( i >= length ){
					return( DECODE_INVALID_UTF16 );
				}
				unsigned int low = p[i] | (p[i + 1] << 8);
				if( low < 0xDC00 || low > 0xDFFF ){
					return( DECODE_INVALID_UTF16 );
				}
				i += 2;
				AppendUtf8( utf8, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00) );
			} else if( unit >= 0xDC00 && unit <= 0xDFFF ){
				return( DECODE_INVALID_UTF16 );
			} else {
				AppendUtf8( utf8, unit );
			}
		}
		return( DECODE_OK );
	}

	// UTF-8（BOM 有無どちらも）
	size_t start = 0;
	if( length >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF ){
		start = 3;
	}
	if( !IsValidUtf8( bytes.data() + start, length - start ) ){
		return( DECODE_INVALID_UTF8 );
	}
	utf8.assign( bytes, start, length - start );
	return( DECODE_OK );
}

} // namespace Text2yks
