//---------------------------------------------------------------------------//
//  FileSystem.h
//      ファイルの読み込みの抽象（中核は Windows API を直接使わない）
//      パスは UTF-8。相対パスはプロセスのカレントからの相対
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_FILESYSTEM_H
#define TEXT2YKS_FILESYSTEM_H

#include <string>

namespace Text2yks {

// ファイルが変わったかを見るための印
struct FileStamp {
	bool               exists;
	unsigned long long size;
	unsigned long long time;    // 更新日時（単位は実装による。比べるだけに使う）

	FileStamp() : exists( false ), size( 0 ), time( 0 ) {}
	bool operator==( const FileStamp& other ) const {
		return( exists == other.exists && size == other.size && time == other.time );
	}
	bool operator!=( const FileStamp& other ) const {
		return( !(*this == other) );
	}
};

class FileSystem {
public:
	virtual ~FileSystem() {}
	// ファイルの中身を全部読む。読めなければ false
	virtual bool      ReadAll( const std::string& path, std::string& bytes ) = 0;
	// ファイルの印を得る（無ければ exists=false）
	virtual FileStamp GetStamp( const std::string& path ) = 0;
};

} // namespace Text2yks

#endif // TEXT2YKS_FILESYSTEM_H
