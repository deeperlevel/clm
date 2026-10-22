package crossLanguageAbstraction

import (
	"crypto/rand"
	"encoding/hex"
	"os"
	"path/filepath"
	"strings"
)

// PathState mimics the dataclass structure from Python
type PathState struct {
	Separator string
}

// NewPathState initializes PathState with the OS-native separator
func NewPathState() PathState {
	return PathState{Separator: string(filepath.Separator)}
}

// ClaPathChangeExtension changes the extension of the path string.
func ClaPathChangeExtension(pathStr, extension string) string {
	// Find the current extension
	ext := filepath.Ext(pathStr)
	// Strip the existing extension
	root := strings.TrimSuffix(pathStr, ext)

	if extension != "" && !strings.HasPrefix(extension, ".") {
		extension = "." + extension
	}
	return root + extension
}

// ClaPathEndsInDirectorySeparator inspects if the path string ends in a directory separator.
func ClaPathEndsInDirectorySeparator(pathStr string) bool {
	if pathStr == "" {
		return false
	}
	return os.IsPathSeparator(pathStr[len(pathStr)-1])
}

// ClaPathExists checks if the path exists on the file system.
func ClaPathExists(pathStr string) bool {
	_, err := os.Stat(pathStr)
	return !os.IsNotExist(err)
}

// ClaPathGetDirectoryName gets the directory component of the path.
func ClaPathGetDirectoryName(pathStr string) string {
	return filepath.Dir(pathStr)
}

// ClaPathGetFullPath returns the absolute path for the specified path.
func ClaPathGetFullPath(pathStr string) (string, error) {
	return filepath.Abs(pathStr)
}

// ClaPathGetFileExtension returns the file extension for the specified path.
func ClaPathGetFileExtension(pathStr string) string {
	return filepath.Ext(pathStr)
}

// ClaPathGetFilename returns the file name for the specified path.
func ClaPathGetFilename(pathStr string) string {
	return filepath.Base(pathStr)
}

// ClaPathGetFileNameWithoutExtension returns the file name without extension.
func ClaPathGetFileNameWithoutExtension(pathStr string) string {
	filename := filepath.Base(pathStr)
	ext := filepath.Ext(filename)
	return strings.TrimSuffix(filename, ext)
}

// ClaPathGetInvalidPathCharacters returns a list of invalid path characters based on OS runtime.
func ClaPathGetInvalidPathCharacters() []string {
	// Evaluates dynamically using a custom compile-time or runtime layout constraint
	// On Windows, specific control codes and special symbols are blocked.
	if string(filepath.Separator) == "\\" {
		return []string{"<", ">", ":", "\"", "|", "?", "*"}
	}
	// Unix systems strictly block just the null character '\x00'
	return []string{"\x00"}
}

// ClaPathGetPathRoot returns the volume name (like "C:") or the system root.
func ClaPathGetPathRoot(pathStr string) string {
	vol := filepath.VolumeName(pathStr)
	if vol != "" {
		return vol
	}
	if filepath.IsAbs(pathStr) {
		return string(filepath.Separator)
	}
	return ""
}

// ClaPathGetRandomFilename returns a cryptographically secure random filename.
func ClaPathGetRandomFilename(extension string) (string, error) {
	bytes := make([]byte, 6)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	randomName := hex.EncodeToString(bytes)

	if extension != "" {
		if !strings.HasPrefix(extension, ".") {
			extension = "." + extension
		}
		return randomName + extension, nil
	}
	return randomName, nil
}

// ClaPathGetRelativePath returns the relative path from one path to another.
func ClaPathGetRelativePath(relativeTo, pathStr string) (string, error) {
	return filepath.Rel(relativeTo, pathStr)
}

// ClaPathGetTemporaryDirectory creates a unique temporary directory inside the specified path.
func ClaPathGetTemporaryDirectory(pathStr string) (string, error) {
	absBase, err := filepath.Abs(pathStr)
	if err != nil {
		return "", err
	}

	// Ensure parent directory infrastructure exists
	if err := os.MkdirAll(absBase, 0755); err != nil {
		return "", err
	}

	// Create unique directory using system pattern matching
	return os.MkdirTemp(absBase, "tmp_*")
}

// ClaPathHasExtension checks if the path contains a file extension.
func ClaPathHasExtension(pathStr string) bool {
	return filepath.Ext(pathStr) != ""
}

// ClaPathIsFullPath checks if the path is an absolute path.
func ClaPathIsFullPath(pathStr string) bool {
	return filepath.IsAbs(pathStr)
}

// ClaPathJoin joins elements into a single path cleanly.
func ClaPathJoin(path1, path2 string) string {
	return filepath.Join(path1, path2)
}

// ClaPathTrimEndingSeparator trims the trailing directory separator if it exists.
func ClaPathTrimEndingSeparator(pathStr string) string {
	if ClaPathEndsInDirectorySeparator(pathStr) && pathStr != string(filepath.Separator) {
		return strings.TrimRight(pathStr, "\\/")
	}
	return pathStr
}

// ClaPathGetDirectorySeparator returns the OS-specific directory separator character string.
func ClaPathGetDirectorySeparator() string {
	return string(filepath.Separator)
}
