package tests

import "testing"

func TestBase(t *testing.T) {
	if result := BaseTests(); result.Failed != 0 {
		t.Fatalf("%d base assertions failed", result.Failed)
	}
}
func TestConsole(t *testing.T) {
	if result := ConsoleTests(); result.Failed != 0 {
		t.Fatalf("%d console assertions failed", result.Failed)
	}
}

func TestStorage(t *testing.T) {
	if result := StorageTests(); result.Failed != 0 {
		t.Fatalf("%d storage assertions failed", result.Failed)
	}
}

func TestString(t *testing.T) {
	if result := StringTests(); result.Failed != 0 {
		t.Fatalf("%d string assertions failed", result.Failed)
	}
}

func TestDictionary(t *testing.T) {
	if result := DictionaryTests(); result.Failed != 0 {
		t.Fatalf("%d dictionary assertions failed", result.Failed)
	}
}
func TestID(t *testing.T) {
	if result := IDTests(); result.Failed != 0 {
		t.Fatalf("%d ID assertions failed", result.Failed)
	}
}

func TestBpa(t *testing.T) {
	if result := BpaTests(); result.Failed != 0 {
		t.Fatalf("%d BPA assertions failed", result.Failed)
	}
}
