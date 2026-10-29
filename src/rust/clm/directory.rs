//! Native Rust CLM directory port: experimental subset.
use std::path::Path;
pub const SUCCESS:i32=0;pub const FAILURE:i32=-1;pub const UNSUPPORTED:i32=-2;pub const STOPPED:i32=1;
pub fn exists_as(path:&str)->i32{if Path::new(path).is_dir(){1}else{0}}
pub fn create_as(path:&str)->i32{if std::fs::create_dir(path).is_ok(){SUCCESS}else{FAILURE}}
pub fn create_recursive_as(path:&str)->i32{if std::fs::create_dir_all(path).is_ok(){SUCCESS}else{FAILURE}}
pub fn delete_as(path:&str)->i32{if std::fs::remove_dir(path).is_ok(){SUCCESS}else{FAILURE}}
fn safe(path:&str)->bool{let p=Path::new(path);if p.as_os_str().is_empty()||p==Path::new("/"){return false}match p.canonicalize(){Ok(v)=>v!=Path::new("/") && !p.symlink_metadata().is_ok_and(|m|m.file_type().is_symlink()),Err(_)=>false}}
pub fn delete_recursive_as(path:&str)->i32{if !safe(path){return FAILURE};if std::fs::remove_dir_all(path).is_ok(){SUCCESS}else{FAILURE}}
pub fn find_entries_as(path:&str)->Result<Vec<String>,i32>{let mut result=vec![];for e in std::fs::read_dir(path).map_err(|_|FAILURE)? {let e=e.map_err(|_|FAILURE)?;result.push(e.path().to_string_lossy().to_string());}Ok(result)}
pub fn find_files_as(path:&str)->Result<Vec<String>,i32>{let mut result=vec![];for e in std::fs::read_dir(path).map_err(|_|FAILURE)? {let e=e.map_err(|_|FAILURE)?;if e.file_type().map_err(|_|FAILURE)?.is_file(){result.push(e.path().to_string_lossy().to_string());}}Ok(result)}
pub fn find_directories_as(path:&str)->Result<Vec<String>,i32>{let mut result=vec![];for e in std::fs::read_dir(path).map_err(|_|FAILURE)? {let e=e.map_err(|_|FAILURE)?;if e.file_type().map_err(|_|FAILURE)?.is_dir(){result.push(e.path().to_string_lossy().to_string());}}Ok(result)}
pub fn get_current_to()->Result<String,i32>{std::env::current_dir().map(|p|p.to_string_lossy().to_string()).map_err(|_|FAILURE)}
pub fn set_current_as(path:&str)->i32{if std::env::set_current_dir(path).is_ok(){SUCCESS}else{FAILURE}}
