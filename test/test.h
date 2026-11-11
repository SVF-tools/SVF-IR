#include <iostream>

#include "Ast.h"
#include "Contracts.h"
#include "Err.h"
#include "Parser.h"
#include "Result.h"

using namespace SVFIR;

/// When expr evaluates to false prints an error message and returns false from
/// enclosing function.
#define TEST(expr)\
    if (!(expr)) {\
        std::cerr\
            << "FAILURE at " << __FILE__ << ":" << __LINE__\
            << " '" << #expr << "'"\
            << std::endl;\
        return false;\
    }

/// Parse source, insisting it is syntactically valid. Often we want to parse
/// some source just to get a Program out for some non-parser test, hence the
/// insist.
Program insistParse(const std::string &source) {
    const Result<Program, Err> &p = parseString(source);
    insist(!is<Err>(p));
    return get<Program>(p);
}
