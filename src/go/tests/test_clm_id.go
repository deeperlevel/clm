package tests

import "github.com/deeperlevel/clm/src/go/pkg/clm"

func IDTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - ID")
	result.Assert(clm.IDIsCaseSensitive() && clm.IDClear() == 0, "default case-sensitive registry")
	result.Assert(clm.IDRegister("host", 1001) == 2 && clm.IDGet("host") == 1001, "register and resolve")
	result.Assert(clm.IDMap("mapped-name", 1002) == 2 && clm.IDRegister("default_host", 1001) == 2, "mapping aliases")
	result.Assert(clm.IDRegister("host", 1002) == -1 && clm.IDRegister("\xc0\xaf", 1003) == -1 && clm.IDRegister("", 1) == -1, "reject duplicate and invalid names")
	value, status := clm.IDTryGet("host")
	result.Assert(status == 1 && value == 1001 && clm.IDGet("HOST") == clm.IDNotFound, "status lookup and exact case")
	result.Assert(clm.IDSet("host", 2001) == 2 && clm.IDGet("default_host") == 1001, "update preserves aliases")
	result.Assert(clm.IDSet("new_name", 3001) == 2 && clm.IDRemove("new_name") == 0 && clm.IDRemove("missing") == -1, "insert through set and remove")
	result.Assert(clm.IDRegister("Host", 4001) == 2 && clm.IDSetCaseSensitive(false) == -1 && clm.IDIsCaseSensitive() && clm.IDGet("host") == 2001, "case collision rolls back")
	result.Assert(clm.IDRemove("Host") == 0 && clm.IDSetCaseSensitive(false) == 0 && clm.IDGet("HOST") == 2001, "enable ASCII case folding")
	result.Assert(clm.IDSet("HOST", 2002) == 2 && clm.IDSetCaseSensitive(true) == 0 && clm.IDGet("host") == 2002 && clm.IDGet("HOST") == clm.IDNotFound, "updates preserve registered spelling")
	result.Assert(clm.IDSetCaseSensitive(false) == 0 && clm.IDRegister("\xc3\x84", 6001) == 2 && clm.IDRegister("\xc3\xa4", 6002) == 2 && clm.IDGet("\xc3\x84") == 6001, "non-ASCII names remain distinct")
	result.Assert(clm.IDRegister("alias", 2002) == 2 && clm.IDUnmap(2002) == 0 && clm.IDGet("host") == clm.IDNotFound && clm.IDGet("alias") == clm.IDNotFound, "unmap every alias")
	result.Assert(clm.IDUnmapName("default_host") == 0, "remove one alias")
	clm.IDGenReset()
	result.Assert(clm.IDGenNext() == 0 && clm.IDGenNext() == 1 && clm.IDGenNext() == 2, "sequential generation")
	clm.IDGenReset()
	result.Assert(clm.IDGenNext() == 0, "reset generator")
	result.Assert(clm.IDRegister("sentinel", clm.IDNotFound) == 2, "sentinel can be mapped")
	value, status = clm.IDTryGet("sentinel")
	result.Assert(status == 1 && value == clm.IDNotFound, "status distinguishes mapped sentinel")
	result.Assert(clm.IDClear() == 0 && !clm.IDIsCaseSensitive() && clm.IDRegister("after-clear", 5001) == 2 && clm.IDGet("AFTER-CLEAR") == 5001, "clear preserves mode")
	result.Assert(clm.IDShutdown() == 0 && clm.IDGet("after-clear") == clm.IDNotFound && clm.IDRegister("after-shutdown", 5002) == 2 && clm.IDShutdown() == 0, "shutdown and lazy recreation")
	result.Assert(clm.IDSetCaseSensitive(true) == 0 && clm.IDShutdown() == 0, "restore default mode")
	CLMTestSummary("ID Tests Summary", result)
	return result
}
