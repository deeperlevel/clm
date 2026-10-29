unit clm_file;
{$mode objfpc}{$H+}
{Native Pascal CLM file port, experimental subset.}
interface
uses Classes, SysUtils;
const CLM_FILE_SUCCESS=0; CLM_FILE_FAILURE=-1; CLM_FILE_UNSUPPORTED=-2;
      CLM_FILE_READ=1; CLM_FILE_WRITE=2; CLM_FILE_APPEND=4; CLM_FILE_CREATE=8; CLM_FILE_TRUNCATE=16; CLM_FILE_EXCLUSIVE=32;
function clm_file_open_as(id: QWord; const path: string; mode: Integer): Integer;
function clm_file_close(id: QWord): Integer;
function clm_file_shutdown: Integer;
function clm_file_is_open(id: QWord): Integer;
function clm_file_exists_as(const path: string): Integer;
function clm_file_read_all_to(const path: string; out bytes: TBytes): Integer;
function clm_file_write_all_from(const path: string; const bytes: TBytes): Integer;
implementation
type PEntry=^TEntry; TEntry=record id:QWord; stream:TFileStream; retired:Boolean; next:PEntry; end;
var head:PEntry=nil;
function find(id:QWord):PEntry;begin Result:=head;while (Result<>nil) and (Result^.id<>id) do Result:=Result^.next;end;
function clm_file_open_as(id:QWord;const path:string;mode:Integer):Integer;
var e:PEntry; flags:Word;
begin
 Result:=-1;
 if (find(id)<>nil) or (path='') or ((mode and 7)=0) or ((mode and (not 63))<>0) or (((mode and 32)<>0) and ((mode and 8)=0)) then Exit;
 if ((mode and 16)<>0) and (((mode and 2)=0) or ((mode and 4)<>0)) then Exit;
 try
   if FileExists(path) then begin if (mode and 32)<>0 then Exit; if (mode and 16)<>0 then flags:=fmCreate else if (mode and 7)=1 then flags:=fmOpenRead else flags:=fmOpenReadWrite;end
   else begin if (mode and 8)=0 then Exit;flags:=fmCreate;end;
   New(e); e^.id:=id;e^.retired:=False;e^.next:=head;
   try e^.stream:=TFileStream.Create(path,flags);except Dispose(e);raise;end;
   head:=e; Result:=0;
 except on E:Exception do Result:=-1;end;
end;
function clm_file_close(id:QWord):Integer;
var e:PEntry;
begin Result:=-1;e:=find(id);if (e=nil) or e^.retired then Exit;
 try e^.stream.Free;e^.stream:=nil;e^.retired:=True;Result:=0;except on E:Exception do begin e^.retired:=True;Result:=-1;end;end;
end;
function clm_file_shutdown:Integer;
var e,n:PEntry;
begin Result:=0;e:=head;while e<>nil do begin n:=e^.next;if not e^.retired then if clm_file_close(e^.id)<>0 then Result:=-1;Dispose(e);e:=n;end;head:=nil;end;
function clm_file_is_open(id:QWord):Integer;
var e:PEntry;begin e:=find(id);if (e<>nil) and (not e^.retired) then Result:=1 else Result:=0;end;
function clm_file_exists_as(const path:string):Integer;begin if FileExists(path) then Result:=1 else Result:=0;end;
function clm_file_read_all_to(const path:string;out bytes:TBytes):Integer;
var s:TFileStream;
begin Result:=-1;try s:=TFileStream.Create(path,fmOpenRead);try SetLength(bytes,s.Size);if Length(bytes)>0 then s.ReadBuffer(bytes[0],Length(bytes));Result:=0;finally s.Free;end;except on E:Exception do Result:=-1;end;end;
function clm_file_write_all_from(const path:string;const bytes:TBytes):Integer;
var s:TFileStream;
begin Result:=-1;try s:=TFileStream.Create(path,fmCreate);try if Length(bytes)>0 then s.WriteBuffer(bytes[0],Length(bytes));Result:=0;finally s.Free;end;except on E:Exception do Result:=-1;end;end;
end.
