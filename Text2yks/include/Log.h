//---------------------------------------------------------------------------//
//  Log.h
//      ログの受け口（中核は UTF-8 の文字列を渡すだけで、出し先は知らない）
//      メッセージは英語で書く
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_LOG_H
#define TEXT2YKS_LOG_H

#include <stddef.h>
#include <string>

namespace Text2yks {

// ログの出し先。message は "Text2yks: error: …" の形の UTF-8（改行は含まない）
typedef void (*LogSink)( const char* message, void* context );

void        SetLogSink( LogSink sink, void* context );
void        LogError( const std::string& message );
void        LogWarning( const std::string& message );
void        LogInfo( const std::string& message );
std::string LogWhere( const std::string& file, int line );
std::string NumberToString( size_t value );

} // namespace Text2yks

#endif // TEXT2YKS_LOG_H
