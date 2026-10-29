require 'securerandom'
module CLMGen
  GENERATION_SUCCESS=0;GENERATION_FAILURE=1
  GENERATION_CASING_MIXED=0;GENERATION_CASING_UPPER=1;GENERATION_CASING_LOWER=2
  SYMBOLS='!@#$%^&*()-_=+[]{};:,.?/|~'
  module_function
  def bounds(lo,hi);lo<=hi ? [lo,hi] : [hi,lo];end
  def integer(min,max,bits,signed)
    low=signed ? -(1 << (bits-1)) : 0
    high=signed ? (1 << (bits-1))-1 : (1 << bits)-1
    raise RangeError unless (low..high).cover?(min) && (low..high).cover?(max)
    lo,hi=bounds(min,max);lo+SecureRandom.random_number(hi-lo+1)
  end
  def clm_gen_random_i8(min,max);integer(min,max,8,true);end
  def clm_gen_random_u8(min,max);integer(min,max,8,false);end
  def clm_gen_random_i16(min,max);integer(min,max,16,true);end
  def clm_gen_random_u16(min,max);integer(min,max,16,false);end
  def clm_gen_random_i32(min,max);integer(min,max,32,true);end
  def clm_gen_random_u32(min,max);integer(min,max,32,false);end
  def clm_gen_random_i64(min,max);integer(min,max,64,true);end
  def clm_gen_random_u64(min,max);integer(min,max,64,false);end
  def clm_gen_random_f64(min,max)
    return Float::NAN unless min.finite? && max.finite?
    lo,hi=bounds(min,max);return lo if lo==hi
    u=SecureRandom.random_number(1<<53).to_f/((1<<53)-1)
    v=lo<0 && hi>0 ? lo*(1-u)+hi*u : lo+(hi-lo)*u
    [[v,lo].max,hi].min
  end
  def clm_gen_random_f32(min,max);[clm_gen_random_f64(min,max)].pack('f').unpack1('f');end
  def check(c);raise ArgumentError unless [0,1,2].include?(c);end
  def pick(a,n);raise ArgumentError if n<0;Array.new(n){a[SecureRandom.random_number(a.length)]}.join;end
  def clm_gen_hex(length,casing)
    check(casing);s=pick(casing==1 ? '0123456789ABCDEF' : '0123456789abcdef',length)
    casing==0 ? s.chars.map{|c| ('a'..'f').cover?(c) && SecureRandom.random_number(2)==1 ? c.upcase : c}.join : s
  end
  def clm_gen_uuid(casing)
    check(casing);b=SecureRandom.random_bytes(16).bytes;b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128
    s=b.pack('C*').unpack1('H*');s=[s[0,8],s[8,4],s[12,4],s[16,4],s[20,12]].join('-')
    return s.upcase if casing==1
    return s.chars.map{|c| ('a'..'f').cover?(c) && SecureRandom.random_number(2)==1 ? c.upcase : c}.join if casing==0
    s
  end
  def clm_gen_alphanum(length,casing)
    check(casing); pick('0123456789'+(casing==1 ? 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' : casing==2 ? 'abcdefghijklmnopqrstuvwxyz' : 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'),length)
  end
  def clm_gen_strong_password(length)
    raise ArgumentError if length<4
    groups=['ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz','0123456789',SYMBOLS]
    a=groups.map{|g| g[SecureRandom.random_number(g.length)]}
    a+=pick(groups.join,length-4).chars
    (a.length-1).downto(1){|i|j=SecureRandom.random_number(i+1);a[i],a[j]=a[j],a[i]}
    a.join
  end
  def to(s, buffer)
    return GENERATION_FAILURE unless buffer.is_a?(String) && buffer.bytesize>=s.bytesize+1
    buffer[0,s.bytesize+1]=s+"\0";GENERATION_SUCCESS
  end
  def clm_gen_hex_to(n,c,b);return GENERATION_FAILURE if n<0;to(clm_gen_hex(n,c),b);rescue ArgumentError;GENERATION_FAILURE;end
  def clm_gen_uuid_to(c,b);to(clm_gen_uuid(c),b);rescue ArgumentError;GENERATION_FAILURE;end
  def clm_gen_alphanum_to(n,c,b);return GENERATION_FAILURE if n<0;to(clm_gen_alphanum(n,c),b);rescue ArgumentError;GENERATION_FAILURE;end
  def clm_gen_strong_password_to(n,b);to(clm_gen_strong_password(n),b);rescue ArgumentError;GENERATION_FAILURE;end
end
