package clm

import (
	"io"
	"os"
	"sync"
)

const (
	FileSuccess     = 0
	FileFailure     = -1
	FileUnsupported = -2
	FileRead        = 1
	FileWrite       = 2
	FileAppend      = 4
	FileCreate      = 8
	FileTruncate    = 16
	FileExclusive   = 32
)

var files = struct {
	sync.Mutex
	open    map[uint64]*os.File
	retired map[uint64]bool
}{open: make(map[uint64]*os.File), retired: make(map[uint64]bool)}

func FileOpenAs(id uint64, path string, mode int) int {
	files.Lock()
	defer files.Unlock()
	if files.retired[id] || files.open[id] != nil || path == "" || mode&7 == 0 || mode&^63 != 0 || (mode&FileExclusive != 0 && mode&FileCreate == 0) || (mode&FileTruncate != 0 && (mode&FileWrite == 0 || mode&FileAppend != 0)) {
		return FileFailure
	}
	flags := 0
	if mode&FileRead != 0 && mode&(FileWrite|FileAppend) != 0 {
		flags = os.O_RDWR
	} else if mode&(FileWrite|FileAppend) != 0 {
		flags = os.O_WRONLY
	} else {
		flags = os.O_RDONLY
	}
	if mode&FileCreate != 0 {
		flags |= os.O_CREATE
	}
	if mode&FileExclusive != 0 {
		flags |= os.O_EXCL
	}
	if mode&FileAppend != 0 {
		flags |= os.O_APPEND
	}
	if mode&FileTruncate != 0 {
		flags |= os.O_TRUNC
	}
	f, e := os.OpenFile(path, flags, 0600)
	if e != nil {
		return FileFailure
	}
	s, e := f.Stat()
	if e != nil || !s.Mode().IsRegular() {
		f.Close()
		return FileFailure
	}
	files.open[id] = f
	return FileSuccess
}
func FileClose(id uint64) int {
	files.Lock()
	f := files.open[id]
	if f == nil {
		files.Unlock()
		return FileFailure
	}
	delete(files.open, id)
	files.retired[id] = true
	files.Unlock()
	if f.Close() != nil {
		return FileFailure
	}
	return FileSuccess
}
func FileShutdown() int {
	files.Lock()
	ids := make([]uint64, 0, len(files.open))
	for id := range files.open {
		ids = append(ids, id)
	}
	files.Unlock()
	result := FileSuccess
	for _, id := range ids {
		if FileClose(id) != 0 {
			result = FileFailure
		}
	}
	files.Lock()
	files.retired = make(map[uint64]bool)
	files.Unlock()
	return result
}
func FileIsOpen(id uint64) bool { files.Lock(); defer files.Unlock(); return files.open[id] != nil }
func FileReadTo(id uint64, dest []byte) (int, int) {
	files.Lock()
	defer files.Unlock()
	f := files.open[id]
	if f == nil {
		return FileFailure, 0
	}
	n, e := f.Read(dest)
	if e != nil && e != io.EOF {
		return FileFailure, n
	}
	return FileSuccess, n
}
func FileWriteFrom(id uint64, data []byte) (int, int) {
	files.Lock()
	defer files.Unlock()
	f := files.open[id]
	if f == nil {
		return FileFailure, 0
	}
	n, e := f.Write(data)
	if e != nil {
		return FileFailure, n
	}
	return FileSuccess, n
}
func FileSeek(id uint64, off int64, origin int) int {
	files.Lock()
	defer files.Unlock()
	f := files.open[id]
	if f == nil {
		return FileFailure
	}
	_, e := f.Seek(off, origin)
	if e != nil {
		return FileFailure
	}
	return FileSuccess
}
func FileTell(id uint64) (int, int64) {
	files.Lock()
	defer files.Unlock()
	f := files.open[id]
	if f == nil {
		return FileFailure, 0
	}
	p, e := f.Seek(0, io.SeekCurrent)
	if e != nil {
		return FileFailure, 0
	}
	return FileSuccess, p
}
func FileSync(id uint64) int {
	files.Lock()
	defer files.Unlock()
	f := files.open[id]
	if f == nil || f.Sync() != nil {
		return FileFailure
	}
	return FileSuccess
}
func FileExistsAs(path string) int {
	s, e := os.Stat(path)
	if os.IsNotExist(e) {
		return 0
	}
	if e != nil {
		return FileFailure
	}
	if s.Mode().IsRegular() {
		return 1
	}
	return 0
}
func FileReadAllTo(path string) (int, []byte) {
	b, e := os.ReadFile(path)
	if e != nil {
		return FileFailure, nil
	}
	return FileSuccess, b
}
func FileWriteAllFrom(path string, data []byte) int {
	if os.WriteFile(path, data, 0600) != nil {
		return FileFailure
	}
	return FileSuccess
}
func FileAppendFrom(path string, data []byte) int {
	f, e := os.OpenFile(path, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
	if e != nil {
		return FileFailure
	}
	_, e = f.Write(data)
	c := f.Close()
	if e != nil || c != nil {
		return FileFailure
	}
	return FileSuccess
}
func FileDeleteAs(path string) int {
	if os.Remove(path) != nil {
		return FileFailure
	}
	return FileSuccess
}
