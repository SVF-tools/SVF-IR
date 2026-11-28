#include <cstdlib>
#include <iostream>
#include <iomanip>

#include "test.h"

#include "cfg.cpp"
#include "dominance.cpp"
#include "linker.cpp"
#include "validation.cpp"
#include "svfir.cpp"
#include "unify-returns.cpp"

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
    success &= runTests("Linker", {testLinkErrs, testLinker});

    // validation.cpp //////////////////////////////////////////////////////////
    success &= runTests(
        "Validation",
        {
            testCheckConstantTypes,
            testCheckTerminatingStmts,
            testCheckDuplicateVars,
            testCheckDuplicateBlocks,
            testCheckVarargStmts,
            testCheckUndeclaredBlocks,
            testCheckUndeclaredGlobals,
            testCheckEntryPredecessors,
            testCheckPhiStmtArgs,
            testCheckLocalDefBeforeUse
        });

    // cfg.cpp /////////////////////////////////////////////////////////////////
    success &= runTests("CFG", {testCfg1, testCfg2, testCfg3, testCfg4});

    // dominance.cpp ///////////////////////////////////////////////////////////
    success &= runTests(
        "Dominance",
        {
            testDominance1,
            testDominance2,
            testDominance3,
            testDominance4,
            testPostDominance1,
            testPostDominance2,
            testPostDominance3
        }
    );

    // dominance.cpp ///////////////////////////////////////////////////////////
    success &= runTests(
        "Return unification",
        {
            testUnifyReturns1,
            testUnifyReturns2,
            testUnifyReturns3,
            testUnifyReturns4
        }
    );

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
