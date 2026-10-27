module main;

import std.stdio;
import clm : clm_bpa_shutdown;
import tests;

int main()
{
    if (!clmTestLogOpen("main.log"))
    {
        stderr.writeln("Failed to open main.log");
        return 1;
    }

    clmBaseTests();
    clmBpaTests();
    clmConsoleTests();
    clmDictionaryTests();
    clmIdTests();
    clmStringTests();
    clmStorageTests();
    overallTestSummary("CLM Tests Summary");
    auto logClosed = clmTestLogClose();
    auto bpaShutdown = clm_bpa_shutdown();
    if (!logClosed || clmTestConsoleFailed || bpaShutdown != 0)
    {
        stderr.writeln("Failed to write test output, close main.log, or shut down BPA");
        return 1;
    }
    return clmTestsFailed == 0 ? 0 : 1;
}