#ifndef TEST_H
#define TEST_H

#include <functional>
#include <iostream>

#include "SVFIR/Ast.h"
#include "SVFIR/Contracts.h"
#include "SVFIR/Err.h"
#include "SVFIR/Parser.h"
#include "SVFIR/Result.h"
#include "SVFIR/TypedProgram.h"

using namespace SVFIR;

using TestFunction = std::function<bool(void)>;

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

/// Parse string src, insisting it is syntactically valid.
/// This is a macro rather than function so insist gets a better location.
#define INSIST_PARSE(src)\
    [](const std::string src_) {\
        const Result<Program, Err> &p = parseString(src_);\
        insist(!is<Err>(p));\
        return get<Program>(p);\
    }(src)

/// Parse string src, insisting it is syntactically and semantically valid.
/// This is a macro rather than function so insist gets a better location.
#define INSIST_TYPED_PARSE(src)\
    [](const std::string src_) {\
        const Result<Program, Err> &p = parseString(src_);\
        insist(!is<Err>(p));\
        const Result<TypedProgram, Err> &tp = typeProgram(get<Program>(p));\
        insist(!is<Err>(tp));\
        return get<TypedProgram>(tp);\
    }(src)

#endif  // TEST_H
