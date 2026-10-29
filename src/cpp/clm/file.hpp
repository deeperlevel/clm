#pragma once
// CLM native C++ file operations: experimental subset; caller-chosen resource IDs.
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
namespace clm::file {
inline constexpr int success=0, failure=-1, unsupported=-2;
inline constexpr int read=1,write=2,append=4,create=8,truncate=16,exclusive=32;
struct resource {std::FILE* handle;int mode;};
inline std::unordered_map<std::uint64_t,resource> registry;
inline std::unordered_set<std::uint64_t> retired;
inline int open_as(std::uint64_t id,const std::string& path,int mode) {
 if (registry.count(id)||retired.count(id)||path.empty()||!(mode&7)||(mode&~63)||(mode&exclusive && !(mode&create))||(mode&truncate && (!(mode&write)||(mode&append))))return failure;
 std::error_code ec;bool exists=std::filesystem::exists(path,ec);if(ec)return failure;
 if(exists && (mode&exclusive))return failure;
 if(!exists && !(mode&create))return failure;
 const char* flags=(mode&append)?((mode&read)?"a+b":"ab"):(mode&truncate)?((mode&read)?"w+b":"wb"):(mode&write)?"r+b":"rb";
 std::FILE* f=std::fopen(path.c_str(),flags);
 if(!f && !exists && (mode&create))f=std::fopen(path.c_str(),"w+b");
 if(!f)return failure;
 if(!std::filesystem::is_regular_file(path,ec)||ec){std::fclose(f);return failure;}
 registry.emplace(id,resource{f,mode});return success;
}
inline int close(std::uint64_t id){auto it=registry.find(id);if(it==registry.end())return failure;auto* f=it->second.handle;registry.erase(it);retired.insert(id);return std::fclose(f)==0?success:failure;}
inline int shutdown(){int result=success;while(!registry.empty())if(close(registry.begin()->first))result=failure;retired.clear();return result;}
inline bool is_open(std::uint64_t id){return registry.count(id)!=0;}
inline int read_to(std::uint64_t id,void* data,std::size_t n,std::size_t* count){auto it=registry.find(id);if(it==registry.end()||!(it->second.mode&read)||(!data&&n)||!count)return failure;*count=std::fread(data,1,n,it->second.handle);return std::ferror(it->second.handle)?failure:success;}
inline int write_from(std::uint64_t id,const void* data,std::size_t n,std::size_t* count){auto it=registry.find(id);if(it==registry.end()||!(it->second.mode&(write|append))||(!data&&n)||!count)return failure;*count=std::fwrite(data,1,n,it->second.handle);return *count==n?success:failure;}
inline int seek(std::uint64_t id,long offset,int origin){auto it=registry.find(id);if(it==registry.end())return failure;return std::fseek(it->second.handle,offset,origin)==0?success:failure;}
inline int tell(std::uint64_t id,long* pos){auto it=registry.find(id);if(it==registry.end()||!pos)return failure;long value=std::ftell(it->second.handle);if(value<0)return failure;*pos=value;return success;}
inline int flush(std::uint64_t id){auto it=registry.find(id);return it!=registry.end()&&std::fflush(it->second.handle)==0?success:failure;}
inline int exists_as(const std::string& p){std::error_code ec;bool yes=std::filesystem::is_regular_file(p,ec);return ec?failure:int(yes);}
inline int read_all_to(const std::string& path,std::vector<unsigned char>& out){std::ifstream in(path,std::ios::binary);if(!in)return failure;std::vector<unsigned char> temp((std::istreambuf_iterator<char>(in)),{});if(in.bad())return failure;out=std::move(temp);return success;}
inline int write_all_from(const std::string& path,const std::vector<unsigned char>& data){std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)return failure;out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));return out?success:failure;}
}
