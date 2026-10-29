//! Native cryptographic generation; direct string and _to APIs (BPA adapters not included).
use getrandom::getrandom;
pub const GENERATION_SUCCESS:i32=0;
pub const GENERATION_FAILURE:i32=1;
pub const GENERATION_CASING_MIXED:i32=0;
pub const GENERATION_CASING_UPPER:i32=1;
pub const GENERATION_CASING_LOWER:i32=2;
const SYMBOLS:&[u8]=b"!@#$%^&*()-_=+[]{};:,.?/|~";
fn rand64()->u64 {let mut b=[0u8;8];getrandom(&mut b).expect("secure RNG failed");u64::from_le_bytes(b)}
fn bounded(bound:u64)->u64 {if bound==0{return rand64()};let threshold=(0u64.wrapping_sub(bound))%bound;loop{let x=rand64();if x>=threshold{return x%bound}}}
pub fn clm_gen_random_u64(mut min:u64,mut max:u64)->u64 {if min>max{std::mem::swap(&mut min,&mut max)}if min==max{return min}min.wrapping_add(bounded(max.wrapping_sub(min).wrapping_add(1)))}
pub fn clm_gen_random_i64(mut min:i64,mut max:i64)->i64 {if min>max{std::mem::swap(&mut min,&mut max)}let sign=1u64<<63;let u=clm_gen_random_u64((min as u64)^sign,(max as u64)^sign);if u<sign {i64::MIN+(u as i64)}else{(u-sign) as i64}}
pub fn clm_gen_random_i8(min:i8,max:i8)->i8{clm_gen_random_i64(min as i64,max as i64) as i8}
pub fn clm_gen_random_u8(min:u8,max:u8)->u8{clm_gen_random_u64(min as u64,max as u64) as u8}
pub fn clm_gen_random_i16(min:i16,max:i16)->i16{clm_gen_random_i64(min as i64,max as i64) as i16}
pub fn clm_gen_random_u16(min:u16,max:u16)->u16{clm_gen_random_u64(min as u64,max as u64) as u16}
pub fn clm_gen_random_i32(min:i32,max:i32)->i32{clm_gen_random_i64(min as i64,max as i64) as i32}
pub fn clm_gen_random_u32(min:u32,max:u32)->u32{clm_gen_random_u64(min as u64,max as u64) as u32}
pub fn clm_gen_random_f64(mut min:f64,mut max:f64)->f64 {if !min.is_finite()||!max.is_finite(){return f64::NAN}if min>max{std::mem::swap(&mut min,&mut max)}if min==max{return min}let unit=(rand64()>>11) as f64/9007199254740991.0;let v=if min<0.0&&max>0.0{min*(1.0-unit)+max*unit}else{min+(max-min)*unit};v.max(min).min(max)}
pub fn clm_gen_random_f32(min:f32,max:f32)->f32{clm_gen_random_f64(min as f64,max as f64) as f32}
fn valid(c:i32)->bool {(0..=2).contains(&c)}
fn pick(a:&[u8])->u8 {a[bounded(a.len() as u64) as usize]}
pub fn clm_gen_hex(n:usize,c:i32)->Option<String>{if !valid(c){return None}let a=if c==1{b"0123456789ABCDEF".as_slice()}else{b"0123456789abcdef".as_slice()};let mut s=String::with_capacity(n);for _ in 0..n{let mut v=pick(a);if c==0&&v>=b'a'&&bounded(2)==1{v-=32}s.push(v as char)}Some(s)}
pub fn clm_gen_alphanum(n:usize,c:i32)->Option<String>{if !valid(c){return None}let a=match c{1=>b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ".as_slice(),2=>b"0123456789abcdefghijklmnopqrstuvwxyz".as_slice(),_=>b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz".as_slice()};Some((0..n).map(|_|pick(a) as char).collect())}
pub fn clm_gen_uuid(c:i32)->Option<String>{if !valid(c){return None}let mut b=[0u8;16];getrandom(&mut b).ok()?;b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;let mut s=String::with_capacity(36);let a=b"0123456789abcdef";for (i,&v) in b.iter().enumerate(){if [4,6,8,10].contains(&i){s.push('-')}for shift in [4,0]{let mut x=a[((v>>shift)&15) as usize];if x>=b'a'&&(c==1||(c==0&&bounded(2)==1)){x-=32}s.push(x as char)}}Some(s)}
pub fn clm_gen_strong_password(n:usize)->Option<String>{if n<4{return None}let groups:[&[u8];4]=[b"ABCDEFGHIJKLMNOPQRSTUVWXYZ",b"abcdefghijklmnopqrstuvwxyz",b"0123456789",SYMBOLS];let alphabet=b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:,.?/|~";let mut b:Vec<u8>=(0..n).map(|i|pick(if i<4{groups[i]}else{alphabet})).collect();for i in (1..n).rev(){let j=bounded((i+1) as u64) as usize;b.swap(i,j)}String::from_utf8(b).ok()}
fn write_to(s:Option<String>,buf:&mut [u8])->i32 {match s{Some(s) if buf.len()>=s.len()+1=>{buf[..s.len()].copy_from_slice(s.as_bytes());buf[s.len()]=0;GENERATION_SUCCESS},_=>GENERATION_FAILURE}}
pub fn clm_gen_hex_to(n:usize,c:i32,buf:&mut[u8])->i32 {if buf.len()<n.saturating_add(1){return GENERATION_FAILURE}write_to(clm_gen_hex(n,c),buf)}
pub fn clm_gen_uuid_to(c:i32,buf:&mut[u8])->i32 {if buf.len()<37{return GENERATION_FAILURE}write_to(clm_gen_uuid(c),buf)}
pub fn clm_gen_alphanum_to(n:usize,c:i32,buf:&mut[u8])->i32 {if buf.len()<n.saturating_add(1){return GENERATION_FAILURE}write_to(clm_gen_alphanum(n,c),buf)}
pub fn clm_gen_strong_password_to(n:usize,buf:&mut[u8])->i32 {if n<4||buf.len()<n.saturating_add(1){return GENERATION_FAILURE}write_to(clm_gen_strong_password(n),buf)}
