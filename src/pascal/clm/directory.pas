unit clm_directory;
{$mode objfpc}{$H+}
{Native Pascal CLM directory port, experimental subset.}
interface
uses Classes,SysUtils;
const CLM_DIRECTORY_SUCCESS=0;CLM_DIRECTORY_FAILURE=-1;CLM_DIRECTORY_UNSUPPORTED=-2;
function clm_directory_exists_as(const path:string):Integer;
function clm_directory_create_as(const path:string):Integer;
function clm_directory_create_recursive_as(const path:string):Integer;
function clm_directory_delete_as(const path:string):Integer;
function clm_directory_find_entries_as(const path:string;results:TStrings):Integer;
function clm_directory_get_current_to(out path:string):Integer;
function clm_directory_set_current_as(const path:string):Integer;
implementation
function clm_directory_exists_as(const path:string):Integer;begin if DirectoryExists(path) then Result:=1 else Result:=0;end;
function clm_directory_create_as(const path:string):Integer;begin if CreateDir(path) then Result:=0 else Result:=-1;end;
function clm_directory_create_recursive_as(const path:string):Integer;begin if ForceDirectories(path) then Result:=0 else Result:=-1;end;
function clm_directory_delete_as(const path:string):Integer;begin if RemoveDir(path) then Result:=0 else Result:=-1;end;
function clm_directory_find_entries_as(const path:string;results:TStrings):Integer;
var sr:TSearchRec;temporary:TStringList;
begin Result:=-1;if results=nil then Exit;temporary:=TStringList.Create;
 try if FindFirst(IncludeTrailingPathDelimiter(path)+'*',faAnyFile,sr)=0 then begin repeat if (sr.Name<>'.') and (sr.Name<>'..') then temporary.Add(IncludeTrailingPathDelimiter(path)+sr.Name);until FindNext(sr)<>0;FindClose(sr);end else if not DirectoryExists(path) then Exit;results.Assign(temporary);Result:=0;finally temporary.Free;end;
end;
function clm_directory_get_current_to(out path:string):Integer;begin path:=GetCurrentDir;if path<>'' then Result:=0 else Result:=-1;end;
function clm_directory_set_current_as(const path:string):Integer;begin if SetCurrentDir(path) then Result:=0 else Result:=-1;end;
end.
