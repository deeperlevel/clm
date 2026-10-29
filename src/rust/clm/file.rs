//! Native Rust CLM file port. Experimental subset, independent from C bindings.
use std::collections::{HashMap, HashSet};
use std::fs::{File, OpenOptions};
use std::io::{Read, Write, Seek, SeekFrom};
use std::path::Path;
pub const SUCCESS:i32=0;pub const FAILURE:i32=-1;pub const UNSUPPORTED:i32=-2;
pub const READ:u32=1;pub const WRITE:u32=2;pub const APPEND:u32=4;pub const CREATE:u32=8;pub const TRUNCATE:u32=16;pub const EXCLUSIVE:u32=32;
pub struct FileRegistry {open:HashMap<u64,(File,u32)>,retired:HashSet<u64>}
impl Default for FileRegistry {fn default()->Self{Self{open:HashMap::new(),retired:HashSet::new()}}}
impl FileRegistry {
 pub fn open_as(&mut self,id:u64,path:&str,mode:u32)->i32 {if self.open.contains_key(&id)||self.retired.contains(&id)||path.is_empty()||mode&7==0||mode & !63!=0||(mode&EXCLUSIVE!=0&&mode&CREATE==0)||(mode&TRUNCATE!=0&&(mode&WRITE==0||mode&APPEND!=0)){return FAILURE;}
 let mut opts=OpenOptions::new();opts.read(mode&READ!=0).write(mode&WRITE!=0).append(mode&APPEND!=0).create(mode&CREATE!=0&&mode&EXCLUSIVE==0).create_new(mode&EXCLUSIVE!=0).truncate(mode&TRUNCATE!=0);
 match opts.open(path){Ok(f)=>{if !f.metadata().is_ok_and(|m|m.is_file()){return FAILURE;}self.open.insert(id,(f,mode));SUCCESS},Err(_)=>FAILURE}}
 pub fn close(&mut self,id:u64)->i32{if self.open.remove(&id).is_none(){return FAILURE;}self.retired.insert(id);SUCCESS}
 pub fn shutdown(&mut self){self.open.clear();self.retired.clear();}
 pub fn is_open(&self,id:u64)->bool{self.open.contains_key(&id)}
 pub fn read_to(&mut self,id:u64,data:&mut [u8])->Result<usize,i32>{let (f,mode)=self.open.get_mut(&id).ok_or(FAILURE)?;if *mode&READ==0{return Err(FAILURE)};f.read(data).map_err(|_|FAILURE)}
 pub fn write_from(&mut self,id:u64,data:&[u8])->Result<usize,i32>{let (f,mode)=self.open.get_mut(&id).ok_or(FAILURE)?;if *mode&(WRITE|APPEND)==0{return Err(FAILURE)};f.write(data).map_err(|_|FAILURE)}
 pub fn seek(&mut self,id:u64,offset:i64,origin:u32)->i32{let (f,_)=match self.open.get_mut(&id){Some(v)=>v,None=>return FAILURE};let from=match origin{0 if offset>=0=>SeekFrom::Start(offset as u64),1=>SeekFrom::Current(offset),2=>SeekFrom::End(offset),_=>return FAILURE};if f.seek(from).is_err(){FAILURE}else{SUCCESS}}
 pub fn sync(&mut self,id:u64)->i32{self.open.get(&id).map_or(FAILURE,|(f,_)|if f.sync_all().is_ok(){SUCCESS}else{FAILURE})}
}
pub fn exists_as(path:&str)->i32{if Path::new(path).is_file(){1}else{0}}
pub fn read_all_to(path:&str)->Result<Vec<u8>,i32>{std::fs::read(path).map_err(|_|FAILURE)}
pub fn write_all_from(path:&str,bytes:&[u8])->i32{if std::fs::write(path,bytes).is_ok(){SUCCESS}else{FAILURE}}
pub fn delete_as(path:&str)->i32{if std::fs::remove_file(path).is_ok(){SUCCESS}else{FAILURE}}
