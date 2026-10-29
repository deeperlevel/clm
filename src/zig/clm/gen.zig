const std=@import("std");
pub const GENERATION_SUCCESS:i32=0;
pub const GENERATION_FAILURE:i32=1;
pub const GENERATION_CASING_MIXED:i32=0;
pub const GENERATION_CASING_UPPER:i32=1;
pub const GENERATION_CASING_LOWER:i32=2;
const symbols="!@#$%^&*()-_=+[]{};:,.?/|~";
fn next64() u64 {var bytes:[8]u8=undefined;std.crypto.random.bytes(&bytes);return std.mem.readInt(u64,&bytes,.little);}
fn bounded(bound:u64) u64 {if(bound==0)return next64();const threshold=(0-%bound)%bound;while(true){const x=next64();if(x>=threshold)return x%bound;}}
pub fn clm_gen_random_u64(a:u64,b:u64) u64 {const lo=@min(a,b);const hi=@max(a,b);if(lo==hi)return lo;return lo+%bounded(hi-%lo+%1);}
pub fn clm_gen_random_i64(a:i64,b:i64) i64 {const sign:u64=1<<63;const lo=@min(a,b);const hi=@max(a,b);const x=clm_gen_random_u64(@as(u64,@bitCast(lo))^sign,@as(u64,@bitCast(hi))^sign);if(x<sign)return std.math.minInt(i64)+@as(i64,@intCast(x));return @intCast(x-sign);}
pub fn clm_gen_random_i8(a:i8,b:i8) i8 {return @intCast(clm_gen_random_i64(a,b));}
pub fn clm_gen_random_u8(a:u8,b:u8) u8 {return @intCast(clm_gen_random_u64(a,b));}
pub fn clm_gen_random_i16(a:i16,b:i16) i16 {return @intCast(clm_gen_random_i64(a,b));}
pub fn clm_gen_random_u16(a:u16,b:u16) u16 {return @intCast(clm_gen_random_u64(a,b));}
pub fn clm_gen_random_i32(a:i32,b:i32) i32 {return @intCast(clm_gen_random_i64(a,b));}
pub fn clm_gen_random_u32(a:u32,b:u32) u32 {return @intCast(clm_gen_random_u64(a,b));}
pub fn clm_gen_random_f64(a:f64,b:f64) f64 {if(!std.math.isFinite(a) or !std.math.isFinite(b))return std.math.nan(f64);const lo=@min(a,b);const hi=@max(a,b);if(lo==hi)return lo;const u=@as(f64,@floatFromInt(next64()>>11))/9007199254740991.0;const v=if(lo<0 and hi>0) lo*(1-u)+hi*u else lo+(hi-lo)*u;return @max(lo,@min(hi,v));}
pub fn clm_gen_random_f32(a:f32,b:f32) f32 {return @floatCast(clm_gen_random_f64(a,b));}
fn valid(c:i32) bool{return c>=0 and c<=2;}
fn pick(alphabet:[]const u8) u8 {return alphabet[@intCast(bounded(alphabet.len))];}
fn generateText(n:usize,c:i32,kind:enum{hex,alphanum},dest:[]u8) i32 {
 if(!valid(c) or dest.len<n+1)return GENERATION_FAILURE;
 const a=if(kind==.hex) (if(c==1) "0123456789ABCDEF" else "0123456789abcdef") else (if(c==1) "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" else if(c==2) "0123456789abcdefghijklmnopqrstuvwxyz" else "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
 for(dest[0..n]) |*v| {v.*=pick(a);if(kind==.hex and c==0 and v.*>='a' and v.*<='f' and bounded(2)==1)v.*-=32;}
 dest[n]=0;return GENERATION_SUCCESS;
}
pub fn clm_gen_hex_to(n:usize,c:i32,buffer:[]u8) i32 {return generateText(n,c,.hex,buffer);}
pub fn clm_gen_alphanum_to(n:usize,c:i32,buffer:[]u8) i32 {return generateText(n,c,.alphanum,buffer);}
pub fn clm_gen_uuid_to(c:i32,buffer:[]u8) i32 {
 if(!valid(c) or buffer.len<37)return GENERATION_FAILURE;
 var b:[16]u8=undefined;std.crypto.random.bytes(&b);b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;
 var p:usize=0;for(b,0..) |v,i| {if(i==4 or i==6 or i==8 or i==10){buffer[p]='-';p+=1;}for([_]u3{4,0}) |shift|{var ch="0123456789abcdef"[(v>>shift)&15];if(ch>='a' and (c==1 or (c==0 and bounded(2)==1)))ch-=32;buffer[p]=ch;p+=1;}}
 buffer[36]=0;return GENERATION_SUCCESS;
}
pub fn clm_gen_strong_password_to(n:usize,buffer:[]u8) i32 {
 if(n<4 or buffer.len<n+1)return GENERATION_FAILURE;
 const groups=[4][]const u8{"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789",symbols};
 const all="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:,.?/|~";
 for(buffer[0..n],0..) |*v,i|{v.*=pick(if(i<4)groups[i] else all);}
 var i=n-1;while(i>0):(i-=1){const j:usize=@intCast(bounded(i+1));std.mem.swap(u8,&buffer[i],&buffer[j]);}
 buffer[n]=0;return GENERATION_SUCCESS;
}
