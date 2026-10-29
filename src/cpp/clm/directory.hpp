#pragma once
// CLM native C++ directory operations: experimental subset.
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <system_error>
namespace clm::directory {
namespace fs=std::filesystem;
inline constexpr int success=0,failure=-1,unsupported=-2,stopped=1;
inline int exists_as(const std::string& path){std::error_code ec;bool yes=fs::is_directory(path,ec);return ec?failure:int(yes);}
inline int create_as(const std::string& path){std::error_code ec;bool yes=fs::create_directory(path,ec);return !ec&&yes?success:failure;}
inline int create_recursive_as(const std::string& path){std::error_code ec;fs::create_directories(path,ec);return ec?failure:success;}
inline int delete_as(const std::string& path){std::error_code ec;bool yes=fs::remove(path,ec);return !ec&&yes?success:failure;}
inline bool safe(const std::string& path){std::error_code ec;auto p=fs::absolute(path,ec).lexically_normal();return !ec&&!p.empty()&&p!=p.root_path()&&!fs::is_symlink(fs::symlink_status(p,ec));}
inline int delete_recursive_as(const std::string& path){if(!safe(path))return failure;std::error_code ec;fs::remove_all(path,ec);return ec?failure:success;}
inline int clear_as(const std::string& path){if(!safe(path))return failure;std::error_code ec;for(fs::directory_iterator it(path,ec);!ec&&it!=fs::directory_iterator();it.increment(ec)){fs::remove_all(it->path(),ec);}return ec?failure:success;}
inline int find_entries_as(const std::string& path,std::vector<std::string>& out,int type=15){std::error_code ec;std::vector<std::string> next;for(fs::directory_iterator it(path,ec);!ec&&it!=fs::directory_iterator();it.increment(ec)){auto st=it->symlink_status(ec);if(ec)break;int kind=fs::is_symlink(st)?4:fs::is_directory(st)?2:fs::is_regular_file(st)?1:8;if(kind&type)next.push_back(it->path().string());}if(ec)return failure;out=std::move(next);return success;}
inline int get_size_as(const std::string& path,std::uint64_t* out){if(!out)return failure;std::error_code ec;std::uint64_t total=0;for(fs::recursive_directory_iterator it(path,fs::directory_options::none,ec);!ec&&it!=fs::recursive_directory_iterator();it.increment(ec)){if(it->is_regular_file(ec)&&!it->is_symlink(ec))total+=it->file_size(ec);if(ec)break;}if(ec)return failure;*out=total;return success;}
inline int move_as(const std::string& a,const std::string& b){std::error_code ec;if(fs::exists(b,ec)||ec)return failure;fs::rename(a,b,ec);return ec?failure:success;}
}
