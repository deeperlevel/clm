module clm.gen;
import std.random : unpredictableSeed;
import std.exception : enforce;
import std.math : isFinite, fmin, fmax;
import std.algorithm.mutation : swap;
import std.file : FileException;
import std.stdio : File;
import std.string : fromStringz;
import core.stdc.stdint;
enum GENERATION_SUCCESS=0;enum GENERATION_FAILURE=1;
enum GENERATION_CASING_MIXED=0;enum GENERATION_CASING_UPPER=1;enum GENERATION_CASING_LOWER=2;
private void secureBytes(ubyte[] dst){auto f=File("/dev/urandom","rb");enforce(f.rawRead(dst).length==dst.length,"secure RNG failed");}
private ulong next64(){ubyte[8] b;secureBytes(b[]);ulong v=0;foreach(i;0..8)v|=cast(ulong)b[i]<<(8*i);return v;}
private ulong bounded(ulong bound){if(bound==0)return next64();auto threshold=(0UL-bound)%bound;while(true){auto n=next64();if(n>=threshold)return n%bound;}}
ulong clm_gen_random_u64(ulong min,ulong max){if(min>max)swap(min,max);if(min==max)return min;return min+bounded(max-min+1);}
long clm_gen_random_i64(long min,long max){if(min>max)swap(min,max);enum ulong sign=1UL<<63;auto u=clm_gen_random_u64((cast(ulong)min)^sign,(cast(ulong)max)^sign);return u<sign?long.min+cast(long)u:cast(long)(u-sign);}
byte clm_gen_random_i8(byte a,byte b){return cast(byte)clm_gen_random_i64(a,b);}
ubyte clm_gen_random_u8(ubyte a,ubyte b){return cast(ubyte)clm_gen_random_u64(a,b);}
short clm_gen_random_i16(short a,short b){return cast(short)clm_gen_random_i64(a,b);}
ushort clm_gen_random_u16(ushort a,ushort b){return cast(ushort)clm_gen_random_u64(a,b);}
int clm_gen_random_i32(int a,int b){return cast(int)clm_gen_random_i64(a,b);}
uint clm_gen_random_u32(uint a,uint b){return cast(uint)clm_gen_random_u64(a,b);}
double clm_gen_random_f64(double min,double max){if(!isFinite(min)||!isFinite(max))return double.nan;if(min>max)swap(min,max);if(min==max)return min;auto u=cast(double)(next64()>>11)/9007199254740991.0;auto v=min<0&&max>0?min*(1-u)+max*u:min+(max-min)*u;return fmax(min,fmin(max,v));}
float clm_gen_random_f32(float a,float b){return cast(float)clm_gen_random_f64(a,b);}
private bool valid(int c){return c>=0&&c<=2;}
private char pick(string a){return a[cast(size_t)bounded(a.length)];}
string clm_gen_hex(int n,int c){enforce(n>=0&&valid(c));auto a=c==1?"0123456789ABCDEF":"0123456789abcdef";char[] s=new char[n];foreach(ref x;s){x=pick(a);if(c==0&&x>='a'&&x<='f'&&bounded(2)==1)x=cast(char)(x-32);}return s.idup;}
string clm_gen_alphanum(int n,int c){enforce(n>=0&&valid(c));auto a=c==1?"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ":c==2?"0123456789abcdefghijklmnopqrstuvwxyz":"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";char[] s=new char[n];foreach(ref x;s)x=pick(a);return s.idup;}
string clm_gen_uuid(int c){enforce(valid(c));ubyte[16] bytes;secureBytes(bytes[]);bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char[] outp;foreach(i,v;bytes){if(i==4||i==6||i==8||i==10)outp~='-';foreach(shift;[4,0]){auto x="0123456789abcdef"[(v>>shift)&15];if(x>='a'&&(c==1||(c==0&&bounded(2)==1)))x=cast(char)(x-32);outp~=x;}}return outp.idup;}
string clm_gen_strong_password(int n){enforce(n>=4);string[4] groups=["ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789","!@#$%^&*()-_=+[]{};:,.?/|~"];auto all=groups[0]~groups[1]~groups[2]~groups[3];char[] s=new char[n];foreach(i,ref x;s)x=pick(i<4?groups[i]:all);for(int i=n-1;i>0;i--){auto j=cast(size_t)bounded(cast(ulong)i+1);swap(s[i],s[j]);}return s.idup;}
private int to(string s,ubyte[] buf){if(buf.length<s.length+1)return GENERATION_FAILURE;foreach(i,x;s)buf[i]=cast(ubyte)x;buf[s.length]=0;return GENERATION_SUCCESS;}
int clm_gen_hex_to(int n,int c,ubyte[] b){if(n<0||!valid(c)||b.length<cast(size_t)n+1)return GENERATION_FAILURE;return to(clm_gen_hex(n,c),b);}
int clm_gen_uuid_to(int c,ubyte[] b){if(!valid(c)||b.length<37)return GENERATION_FAILURE;return to(clm_gen_uuid(c),b);}
int clm_gen_alphanum_to(int n,int c,ubyte[] b){if(n<0||!valid(c)||b.length<cast(size_t)n+1)return GENERATION_FAILURE;return to(clm_gen_alphanum(n,c),b);}
int clm_gen_strong_password_to(int n,ubyte[] b){if(n<4||b.length<cast(size_t)n+1)return GENERATION_FAILURE;return to(clm_gen_strong_password(n),b);}
