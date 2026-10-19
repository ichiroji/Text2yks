//---------------------------------------------------------------------------//
//  FileSystem_Win.h
//      FileSystem の Windows 版（UTF-8 のパスを UTF-16 にして Win32 API で読む）
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_FILESYSTEM_WIN_H
#define TEXT2YKS_FILESYSTEM_WIN_H

#include <string>
#include "FileSystem.h"

namespace Text2yks {

class FileSystemWin : public FileSystem {
public:
	virtual bool      ReadAll( const std::string& path, std::string& bytes );
	virtual FileStamp GetStamp( const std::string& path );
};

std::wstring Utf8ToWide( const std::string& text );

} // namespace Text2yks

#endif // TEXT2YKS_FILESYSTEM_WIN_H
