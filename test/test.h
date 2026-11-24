#include <iostream>

#include "SVFIR/Ast.h"
#include "SVFIR/Contracts.h"
#include "SVFIR/Err.h"
#include "SVFIR/Parser.h"
#include "SVFIR/Result.h"
#include "SVFIR/TypedProgram.h"

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

/// Parse source, insisting it is syntactically valid.
Program insistParse(const std::string &source) {
    const Result<Program, Err> &p = parseString(source);
    insist(!is<Err>(p));
    return get<Program>(p);
}

/// Parse source, insisting it is syntactically and semantically valid.
TypedProgram insistTypedParse(const std::string &source) {
    const Result<Program, Err> &p = parseString(source);
    insist(!is<Err>(p));
    const Result<TypedProgram, Err> &tp = typeProgram(get<Program>(p));
    insist(!is<Err>(tp));
    return get<TypedProgram>(tp);
}
