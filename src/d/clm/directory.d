module clm_directory;
// Native D CLM directory port: experimental subset.
import std.file;
import std.path : buildPath, absolutePath;
const int SUCCESS=0, FAILURE=-1, UNSUPPORTED=-2, STOPPED=1;
int exists_as(string path){try{return isDir(path)?1:0;}catch(Exception){return FAILURE;}}
int create_as(string path){try{mkdir(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int create_recursive_as(string path){try{mkdirRecurse(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int delete_as(string path){try{rmdir(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int delete_recursive_as(string path){try{auto p=absolutePath(path);if(p=="/"||isSymlink(path))return FAILURE;rmdirRecurse(path);return SUCCESS;}catch(Exception){return FAILURE;}}
int move_as(string a,string b){try{if(exists(b))return FAILURE;rename(a,b);return SUCCESS;}catch(Exception){return FAILURE;}}
int find_entries_as(string path,out string[] result){try{foreach(e;dirEntries(path,SpanMode.shallow))result~=e.name;return SUCCESS;}catch(Exception){return FAILURE;}}
int find_files_as(string path,out string[] result){try{foreach(e;dirEntries(path,SpanMode.shallow))if(e.isFile && !e.isSymlink)result~=e.name;return SUCCESS;}catch(Exception){return FAILURE;}}
int find_directories_as(string path,out string[] result){try{foreach(e;dirEntries(path,SpanMode.shallow))if(e.isDir && !e.isSymlink)result~=e.name;return SUCCESS;}catch(Exception){return FAILURE;}}
