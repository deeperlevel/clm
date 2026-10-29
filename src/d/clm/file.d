module clm_file;
// Native D CLM file port: experimental subset.
import std.stdio : File;
import std.file : exists, isFile, read, write, remove;
import std.exception : enforce;
import std.array : appender;
const int SUCCESS=0, FAILURE=-1, UNSUPPORTED=-2;
const int READ=1, WRITE=2, APPEND=4, CREATE=8, TRUNCATE=16, EXCLUSIVE=32;
private File[ulong] registry;
private bool[ulong] retired;
int open_as(ulong id, string path, int mode) {
    if ((id in registry) !is null || (id in retired) !is null || !path.length || !(mode&7) || (mode & ~63) || ((mode&EXCLUSIVE) && !(mode&CREATE)) || ((mode&TRUNCATE) && (!(mode&WRITE)||(mode&APPEND)))) return FAILURE;
    try {
        bool present=exists(path);
        if ((present&&(mode&EXCLUSIVE)) || (!present&&!(mode&CREATE))) return FAILURE;
        string flags=(mode&APPEND)?"a+":(mode&TRUNCATE)?"w+":(mode&WRITE)?"r+":"r";
        auto f=File(path, flags);
        registry[id]=f;
        return SUCCESS;
    } catch(Exception) {return FAILURE;}
}
int close(ulong id) {
    auto p=id in registry; if(p is null)return FAILURE;
    try {p.close();registry.remove(id);retired[id]=true;return SUCCESS;} catch(Exception) {registry.remove(id);retired[id]=true;return FAILURE;}
}
int shutdown(){int result=SUCCESS;foreach(id;registry.keys.dup)if(close(id)!=SUCCESS)result=FAILURE;retired.clear();return result;}
int is_open(ulong id){return (id in registry) is null?0:1;}
int exists_as(string path){try{return isFile(path)?1:0;}catch(Exception){return FAILURE;}}
int delete_as(string path){try{remove(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int read_all_to(string path,out ubyte[] data){try{data=cast(ubyte[])read(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int write_all_from(string path,const(ubyte)[] data){try{write(path,data);return SUCCESS;}catch(Exception){return FAILURE;}}
