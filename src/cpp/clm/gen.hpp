#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <random>
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#else
#include <cstdio>
#endif
namespace clm::gen {
inline constexpr int GENERATION_SUCCESS=0,GENERATION_FAILURE=1;
inline constexpr int GENERATION_CASING_MIXED=0,GENERATION_CASING_UPPER=1,GENERATION_CASING_LOWER=2;
inline bool bytes(void* buf,std::size_t n){
#if defined(_WIN32)
 return n<=std::numeric_limits<ULONG>::max() && BCryptGenRandom(nullptr,static_cast<PUCHAR>(buf),static_cast<ULONG>(n),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0;
#else
 auto* f=std::fopen("/dev/urandom","rb");if(!f)return false;auto count=std::fread(buf,1,n,f);std::fclose(f);return count==n;
#endif
}
inline std::uint64_t random_u64(){std::uint64_t v;if(!bytes(&v,sizeof(v)))throw std::runtime_error("system RNG unavailable");return v;}
inline std::uint64_t bounded(std::uint64_t bound){if(bound==0)return random_u64();auto threshold=(std::uint64_t(0)-bound)%bound;for(;;){auto v=random_u64();if(v>=threshold)return v%bound;}}
inline std::uint64_t clm_gen_random_u64(std::uint64_t lo,std::uint64_t hi){if(lo>hi)std::swap(lo,hi);if(lo==hi)return lo;return lo+bounded(hi-lo+1);}
inline std::int64_t clm_gen_random_i64(std::int64_t lo,std::int64_t hi){if(lo>hi)std::swap(lo,hi);constexpr std::uint64_t sign=std::uint64_t(1)<<63;auto u=clm_gen_random_u64(std::uint64_t(lo)^sign,std::uint64_t(hi)^sign);return u<sign ? std::numeric_limits<std::int64_t>::min()+std::int64_t(u) : std::int64_t(u-sign);}
#define CLM_GEN_SIGNED(bits) inline std::int##bits##_t clm_gen_random_i##bits(std::int##bits##_t a,std::int##bits##_t b){return static_cast<std::int##bits##_t>(clm_gen_random_i64(a,b));}
#define CLM_GEN_UNSIGNED(bits) inline std::uint##bits##_t clm_gen_random_u##bits(std::uint##bits##_t a,std::uint##bits##_t b){return static_cast<std::uint##bits##_t>(clm_gen_random_u64(a,b));}
CLM_GEN_SIGNED(8) CLM_GEN_SIGNED(16) CLM_GEN_SIGNED(32)
CLM_GEN_UNSIGNED(8) CLM_GEN_UNSIGNED(16) CLM_GEN_UNSIGNED(32)
#undef CLM_GEN_SIGNED
#undef CLM_GEN_UNSIGNED
inline double clm_gen_random_f64(double lo,double hi){if(!std::isfinite(lo)||!std::isfinite(hi))return std::numeric_limits<double>::quiet_NaN();if(lo>hi)std::swap(lo,hi);if(lo==hi)return lo;double u=double(random_u64()>>11)/9007199254740991.0;double v=lo<0&&hi>0?lo*(1-u)+hi*u:lo+(hi-lo)*u;return std::clamp(v,lo,hi);}
inline float clm_gen_random_f32(float a,float b){return float(clm_gen_random_f64(a,b));}
inline bool casing_ok(int c){return c>=0&&c<=2;}
inline char pick(const char* alphabet){auto len=std::strlen(alphabet);return alphabet[bounded(len)];}
inline std::string clm_gen_hex(int n,int casing){if(n<0||!casing_ok(casing))throw std::invalid_argument("length or casing");std::string s;s.reserve(n);const char* a=casing==1?"0123456789ABCDEF":"0123456789abcdef";for(int i=0;i<n;i++){char c=pick(a);if(casing==0&&c>='a'&&c<='f'&&bounded(2))c=char(c-'a'+'A');s.push_back(c);}return s;}
inline std::string clm_gen_alphanum(int n,int casing){if(n<0||!casing_ok(casing))throw std::invalid_argument("length or casing");const char* a=casing==1?"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ":casing==2?"0123456789abcdefghijklmnopqrstuvwxyz":"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";std::string s;s.reserve(n);for(int i=0;i<n;i++)s.push_back(pick(a));return s;}
inline std::string clm_gen_uuid(int casing){if(!casing_ok(casing))throw std::invalid_argument("casing");std::array<unsigned char,16>b;if(!bytes(b.data(),b.size()))throw std::runtime_error("RNG unavailable");b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;std::string s;s.reserve(36);const char* hex="0123456789abcdef";for(int i=0;i<16;i++){if(i==4||i==6||i==8||i==10)s.push_back('-');for(int shift: {4,0}){char c=hex[(b[i]>>shift)&15];if(c>='a'&&(casing==1||(casing==0&&bounded(2))))c=char(c-'a'+'A');s.push_back(c);}}return s;}
inline std::string clm_gen_strong_password(int n){if(n<4)throw std::invalid_argument("length");const char* g[]={"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789","!@#$%^&*()-_=+[]{};:,.?/|~"};const char* all="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:,.?/|~";std::string s;s.reserve(n);for(int i=0;i<n;i++)s.push_back(pick(i<4?g[i]:all));for(int i=n-1;i>0;i--){auto j=bounded(std::uint64_t(i)+1);std::swap(s[i],s[j]);}return s;}
inline int write_to(const std::string& s,char* buffer,std::size_t cap){if(!buffer||cap<s.size()+1)return GENERATION_FAILURE;std::memcpy(buffer,s.c_str(),s.size()+1);return GENERATION_SUCCESS;}
inline int clm_gen_hex_to(int n,int c,char* b,std::size_t cap){if(n<0||cap<std::size_t(n)+1)return GENERATION_FAILURE;try{return write_to(clm_gen_hex(n,c),b,cap);}catch(...){return GENERATION_FAILURE;}}
inline int clm_gen_alphanum_to(int n,int c,char* b,std::size_t cap){if(n<0||cap<std::size_t(n)+1)return GENERATION_FAILURE;try{return write_to(clm_gen_alphanum(n,c),b,cap);}catch(...){return GENERATION_FAILURE;}}
inline int clm_gen_uuid_to(int c,char* b,std::size_t cap){if(cap<37)return GENERATION_FAILURE;try{return write_to(clm_gen_uuid(c),b,cap);}catch(...){return GENERATION_FAILURE;}}
inline int clm_gen_strong_password_to(int n,char* b,std::size_t cap){if(n<4||cap<std::size_t(n)+1)return GENERATION_FAILURE;try{return write_to(clm_gen_strong_password(n),b,cap);}catch(...){return GENERATION_FAILURE;}}
}
