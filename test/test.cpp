#include <cstdlib>
#include <iostream>
#include <iomanip>

#include "test.h"

#include "linker.cpp"
#include "validation.cpp"
#include "svfir.cpp"

/// Runs all of the tests in tests, in order, printing a message at the end.
/// Note, testFunctions using TEST do short-circuit but runTests going through
/// the vector tests will not. Similarly with printing messages in the middle.
/// Returns true if all tests passed, otherwise false.
bool runTests(const std::string &title, const std::vector<TestFunction> &tests) {
    unsigned int successes = 0;
    unsigned int failures = 0;
    for (const TestFunction &tf : tests) {
        if (tf()) { ++successes; }
        else { ++failures; }
    }

    std::cout
        << std::setw(25) << title << std::setw(0) << " | "
        << successes << " passed, " << failures << " failed."
        << std::endl;

    return failures == 0;
}

int main(void) {
    bool success = true;

    // svfir.cpp ///////////////////////////////////////////////////////////////
    success &= runTests("roundtrip", {roundtrip});

    // linker.cpp //////////////////////////////////////////////////////////////
    success &= runTests("Linker tests", {testLinkErrs, testLinker});

    // validation.cpp //////////////////////////////////////////////////////////
    success &= runTests(
        "Validation tests",
        {
            testCheckConstantTypes,
            testCheckTerminatingStmts,
            testCheckDuplicateDecs,
            testCheckVarargStmts,
            testCheckUndeclaredBlocks,
            testCheckUndeclaredGlobals,
            testCheckEntryPredecessors,
            testCheckPhiStmtArgs,
            testCheckLocalDefBeforeUse
        });

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
