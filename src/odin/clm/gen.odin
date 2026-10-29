package clm_gen
import "core:crypto/rand"
GENERATION_SUCCESS :: 0
GENERATION_FAILURE :: 1
GENERATION_CASING_MIXED :: 0
GENERATION_CASING_UPPER :: 1
GENERATION_CASING_LOWER :: 2
SYMBOLS :: "!@#$%^&*()-_=+[]{};:,.?/|~"
// This file uses OS-backed crypto randomness; check your Odin compiler stdlib version.
next64 :: proc() -> u64 {b:[8]byte;rand.bytes(b[:]);v:u64=0;for i in 0..<8 {v |= u64(b[i]) << u64(i*8)};return v}
bounded :: proc(bound:u64) -> u64 {if bound==0{return next64()};threshold:=(u64(0)-bound)%bound;for {v:=next64();if v>=threshold{return v%bound}}}
clm_gen_random_u64 :: proc(min,max:u64) -> u64 {lo,hi:=min,max;if lo>hi{lo,hi=hi,lo};if lo==hi{return lo};return lo+bounded(hi-lo+1)}
clm_gen_random_i64 :: proc(min,max:i64) -> i64 {lo,hi:=min,max;if lo>hi{lo,hi=hi,lo};sign:=u64(1)<<63;v:=clm_gen_random_u64(u64(lo)^sign,u64(hi)^sign);if v<sign{return i64(-9223372036854775807-1)+i64(v)};return i64(v-sign)}
clm_gen_random_i8 :: proc(a,b:i8) -> i8 {return i8(clm_gen_random_i64(i64(a),i64(b)))}
clm_gen_random_u8 :: proc(a,b:u8) -> u8 {return u8(clm_gen_random_u64(u64(a),u64(b)))}
clm_gen_random_i16 :: proc(a,b:i16) -> i16 {return i16(clm_gen_random_i64(i64(a),i64(b)))}
clm_gen_random_u16 :: proc(a,b:u16) -> u16 {return u16(clm_gen_random_u64(u64(a),u64(b)))}
clm_gen_random_i32 :: proc(a,b:i32) -> i32 {return i32(clm_gen_random_i64(i64(a),i64(b)))}
clm_gen_random_u32 :: proc(a,b:u32) -> u32 {return u32(clm_gen_random_u64(u64(a),u64(b)))}
clm_gen_random_f64 :: proc(a,b:f64) -> f64 {lo,hi:=a,b;if lo>hi{lo,hi=hi,lo};if lo==hi{return lo};u:=f64(next64()>>11)/9007199254740991.0;v:=lo+(hi-lo)*u;if lo<0&&hi>0{v=lo*(1-u)+hi*u};if v<lo{return lo};if v>hi{return hi};return v}
clm_gen_random_f32 :: proc(a,b:f32) -> f32{return f32(clm_gen_random_f64(f64(a),f64(b)))}
pick :: proc(alphabet:string) -> byte {return alphabet[int(bounded(u64(len(alphabet))))]}
clm_gen_hex_to :: proc(n,c:int,buf:[]byte) -> int {if n<0||c<0||c>2||len(buf)<n+1{return GENERATION_FAILURE};a:="0123456789abcdef";if c==1{a="0123456789ABCDEF"};for i in 0..<n{v:=pick(a);if c==0&&v>='a'&&v<='f'&&bounded(2)==1{v-=32};buf[i]=v};buf[n]=0;return GENERATION_SUCCESS}
clm_gen_alphanum_to :: proc(n,c:int,buf:[]byte) -> int {if n<0||c<0||c>2||len(buf)<n+1{return GENERATION_FAILURE};a:="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";if c==1{a="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"}else if c==2{a="0123456789abcdefghijklmnopqrstuvwxyz"};for i in 0..<n{buf[i]=pick(a)};buf[n]=0;return GENERATION_SUCCESS}
clm_gen_uuid_to :: proc(c:int,buf:[]byte) -> int {if c<0||c>2||len(buf)<37{return GENERATION_FAILURE};b:[16]byte;rand.bytes(b[:]);b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;p:=0;for i in 0..<16{if i==4||i==6||i==8||i==10{buf[p]='-';p+=1};for shift in 0..<2{s:=u8(4);if shift==1{s=0};v:="0123456789abcdef"[int((b[i]>>s)&15)];if v>='a'&&(c==1||(c==0&&bounded(2)==1)){v-=32};buf[p]=v;p+=1}};buf[36]=0;return GENERATION_SUCCESS}
clm_gen_strong_password_to :: proc(n:int,buf:[]byte) -> int {if n<4||len(buf)<n+1{return GENERATION_FAILURE};groups:=[4]string{"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789",SYMBOLS};all:="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:,.?/|~";for i in 0..<n{a:=all;if i<4{a=groups[i]};buf[i]=pick(a)};for i:=n-1;i>0;i-=1{j:=int(bounded(u64(i+1)));buf[i],buf[j]=buf[j],buf[i]};buf[n]=0;return GENERATION_SUCCESS}
