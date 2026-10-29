package clm
import ("crypto/rand";"encoding/binary";"math";"math/big")
const (GenerationSuccess=0;GenerationFailure=1;GenerationCasingMixed=0;GenerationCasingUpper=1;GenerationCasingLower=2)
const symbols="!@#$%^&*()-_=+[]{};:,.?/|~"
func randU64() uint64 {var b [8]byte;if _,err:=rand.Read(b[:]);err!=nil{panic(err)};return binary.LittleEndian.Uint64(b[:])}
func bounded(bound uint64) uint64 {if bound==0{return randU64()};threshold:=(uint64(0)-bound)%bound;for {v:=randU64();if v>=threshold{return v%bound}}}
func ClmGenRandomU64(min,max uint64) uint64 {if min>max{min,max=max,min};if min==max{return min};return min+bounded(max-min+1)}
func ClmGenRandomI64(min,max int64) int64 {if min>max{min,max=max,min};a,b:=uint64(min)^(1<<63),uint64(max)^(1<<63);u:=ClmGenRandomU64(a,b)^(1<<63);return int64(u)}
func ClmGenRandomI8(min,max int8) int8 {return int8(ClmGenRandomI64(int64(min),int64(max)))}
func ClmGenRandomU8(min,max uint8) uint8 {return uint8(ClmGenRandomU64(uint64(min),uint64(max)))}
func ClmGenRandomI16(min,max int16) int16 {return int16(ClmGenRandomI64(int64(min),int64(max)))}
func ClmGenRandomU16(min,max uint16) uint16 {return uint16(ClmGenRandomU64(uint64(min),uint64(max)))}
func ClmGenRandomI32(min,max int32) int32 {return int32(ClmGenRandomI64(int64(min),int64(max)))}
func ClmGenRandomU32(min,max uint32) uint32 {return uint32(ClmGenRandomU64(uint64(min),uint64(max)))}
func ClmGenRandomF64(min,max float64) float64 {if math.IsNaN(min)||math.IsNaN(max)||math.IsInf(min,0)||math.IsInf(max,0){return math.NaN()};if min>max{min,max=max,min};if min==max{return min};u:=float64(randU64()>>11)/9007199254740991.0;v:=0.0;if min<0 && max>0{v=min*(1-u)+max*u}else{v=min+(max-min)*u};return math.Max(min,math.Min(max,v))}
func ClmGenRandomF32(min,max float32) float32 {return float32(ClmGenRandomF64(float64(min),float64(max)))}
func validCasing(c int) bool {return c>=0&&c<=2}
func pick(a string) byte {return a[bounded(uint64(len(a)))]}
func build(n int,a string) string {if n<0{panic("negative length")};v:=make([]byte,n);for i:=range v{v[i]=pick(a)};return string(v)}
func ClmGenHex(length,casing int) (string,bool) {if !validCasing(casing)||length<0{return "",false};a:="0123456789abcdef";if casing==1{a="0123456789ABCDEF"};b:=[]byte(build(length,a));if casing==0{for i,c:=range b {if c>='a'&&c<='f'&&bounded(2)==1{b[i]=c-'a'+'A'}}};return string(b),true}
func ClmGenUUID(casing int) (string,bool) {if !validCasing(casing){return "",false};var b [16]byte;if _,err:=rand.Read(b[:]);err!=nil{return "",false};b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;const low="0123456789abcdef";const up="0123456789ABCDEF";out:=make([]byte,36);p:=0;for i,v:=range b{if i==4||i==6||i==8||i==10{out[p]='-';p++};for _,shift:=range []uint{4,0}{c:=low[(v>>shift)&15];if casing==1||(casing==0&&c>='a'&&bounded(2)==1){c=up[(v>>shift)&15]};out[p]=c;p++}};return string(out),true}
func ClmGenAlphanum(length,casing int)(string,bool){if !validCasing(casing)||length<0{return "",false};a:="0123456789";if casing!=2{a+="ABCDEFGHIJKLMNOPQRSTUVWXYZ"};if casing!=1{a+="abcdefghijklmnopqrstuvwxyz"};return build(length,a),true}
func ClmGenStrongPassword(length int)(string,bool){if length<4{return "",false};groups:=[]string{"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789",symbols};b:=make([]byte,length);for i:=0;i<length;i++ {if i<4{b[i]=pick(groups[i])}else{b[i]=pick(groups[0]+groups[1]+groups[2]+groups[3])}};for i:=length-1;i>0;i--{j:=int(bounded(uint64(i+1)));b[i],b[j]=b[j],b[i]};return string(b),true}
func writeTo(s string,ok bool,dst []byte)int{if !ok||len(dst)<len(s)+1{return GenerationFailure};copy(dst,s);dst[len(s)]=0;return GenerationSuccess}
func ClmGenHexTo(n,c int,dst []byte)int {if n<0||len(dst)<n+1{return GenerationFailure};s,ok:=ClmGenHex(n,c);return writeTo(s,ok,dst)}
func ClmGenUUIDTo(c int,dst []byte)int {if len(dst)<37{return GenerationFailure};s,ok:=ClmGenUUID(c);return writeTo(s,ok,dst)}
func ClmGenAlphanumTo(n,c int,dst []byte)int {if n<0||len(dst)<n+1{return GenerationFailure};s,ok:=ClmGenAlphanum(n,c);return writeTo(s,ok,dst)}
func ClmGenStrongPasswordTo(n int,dst []byte)int {if n<4||len(dst)<n+1{return GenerationFailure};s,ok:=ClmGenStrongPassword(n);return writeTo(s,ok,dst)}
var _ = big.NewInt
