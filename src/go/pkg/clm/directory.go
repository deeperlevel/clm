package clm

import (
	"os"
	"path/filepath"
	"strings"
)

const (
	DirectorySuccess     = 0
	DirectoryFailure     = -1
	DirectoryUnsupported = -2
	DirectoryStopped     = 1
)

func DirectoryExistsAs(path string) int {
	s, e := os.Stat(path)
	if os.IsNotExist(e) {
		return 0
	}
	if e != nil {
		return DirectoryFailure
	}
	if s.IsDir() {
		return 1
	}
	return 0
}
func DirectoryCreateAs(path string) int {
	if os.Mkdir(path, 0700) != nil {
		return DirectoryFailure
	}
	return 0
}
func DirectoryCreateRecursiveAs(path string) int {
	if os.MkdirAll(path, 0700) != nil {
		return DirectoryFailure
	}
	return 0
}
func DirectoryDeleteAs(path string) int {
	if os.Remove(path) != nil {
		return DirectoryFailure
	}
	return 0
}
func directorySafe(path string) bool {
	p, e := filepath.Abs(path)
	if e != nil {
		return false
	}
	v, e := os.Lstat(p)
	if e != nil {
		return false
	}
	return p != filepath.Clean(string(os.PathSeparator)) && p != filepath.VolumeName(p)+string(os.PathSeparator) && v.Mode()&os.ModeSymlink == 0
}
func DirectoryDeleteRecursiveAs(path string) int {
	if !directorySafe(path) {
		return DirectoryFailure
	}
	if os.RemoveAll(path) != nil {
		return DirectoryFailure
	}
	return 0
}
func DirectoryClearAs(path string) int {
	if !directorySafe(path) {
		return DirectoryFailure
	}
	entries, e := os.ReadDir(path)
	if e != nil {
		return DirectoryFailure
	}
	for _, entry := range entries {
		if os.RemoveAll(filepath.Join(path, entry.Name())) != nil {
			return DirectoryFailure
		}
	}
	return 0
}
func DirectoryFindEntriesAs(path string) (int, []string) {
	entries, e := os.ReadDir(path)
	if e != nil {
		return DirectoryFailure, nil
	}
	result := make([]string, 0, len(entries))
	for _, entry := range entries {
		result = append(result, filepath.Join(path, entry.Name()))
	}
	return 0, result
}
func DirectoryFindFilesAs(path string) (int, []string) {
	entries, e := os.ReadDir(path)
	if e != nil {
		return DirectoryFailure, nil
	}
	result := []string{}
	for _, entry := range entries {
		if entry.Type().IsRegular() {
			result = append(result, filepath.Join(path, entry.Name()))
		}
	}
	return 0, result
}
func DirectoryFindDirectoriesAs(path string) (int, []string) {
	entries, e := os.ReadDir(path)
	if e != nil {
		return DirectoryFailure, nil
	}
	result := []string{}
	for _, entry := range entries {
		if entry.IsDir() {
			result = append(result, filepath.Join(path, entry.Name()))
		}
	}
	return 0, result
}
func DirectoryGetCurrentTo() (int, string) {
	p, e := os.Getwd()
	if e != nil {
		return DirectoryFailure, ""
	}
	return 0, p
}
func DirectorySetCurrentAs(path string) int {
	if os.Chdir(path) != nil {
		return DirectoryFailure
	}
	return 0
}
func DirectoryCreateTemporaryTo(path string) (int, string) {
	p, e := os.MkdirTemp(path, "clm-")
	if e != nil {
		return DirectoryFailure, ""
	}
	return 0, p
}
func DirectoryGetSizeAs(path string) (int, uint64) {
	var total uint64
	e := filepath.WalkDir(path, func(p string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.Type().IsRegular() {
			i, e := d.Info()
			if e != nil {
				return e
			}
			total += uint64(i.Size())
		}
		return nil
	})
	if e != nil {
		return DirectoryFailure, 0
	}
	return 0, total
}
func DirectoryCopyAs(src, dst string, overwrite bool) int {
	a, e := filepath.Abs(src)
	if e != nil {
		return -1
	}
	b, e := filepath.Abs(dst)
	if e != nil {
		return -1
	}
	rel, e := filepath.Rel(a, b)
	if e == nil && (rel == "." || (rel != ".." && !strings.HasPrefix(rel, ".."+string(os.PathSeparator)))) {
		return -1
	}
	if !overwrite {
		if _, e := os.Lstat(dst); e == nil {
			return -1
		}
	}
	e = filepath.WalkDir(src, func(p string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.Type()&os.ModeSymlink != 0 {
			return os.ErrInvalid
		}
		r, _ := filepath.Rel(src, p)
		t := filepath.Join(dst, r)
		if d.IsDir() {
			return os.MkdirAll(t, 0700)
		}
		if !d.Type().IsRegular() {
			return os.ErrInvalid
		}
		buf, e := os.ReadFile(p)
		if e != nil {
			return e
		}
		return os.WriteFile(t, buf, 0600)
	})
	if e != nil {
		return DirectoryFailure
	}
	return 0
}
