#ifndef TEST_H
#define TEST_H

#include <functional>
#include <iostream>
#include <sstream>

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

/// Strip the common margin to each line in s. For the first and last lines,
/// if it is empty or contains space characters only, it is removed.
std::string strip(const std::string &s) {
    std::vector<std::string> lines;
    std::stringstream ss = std::stringstream(s);
    for (std::string l; std::getline(ss, l, '\n');) { lines.push_back(l); }

    if (!lines.empty()) {
        if (lines.front().find_first_not_of(' ') == std::string::npos) {
            lines.erase(lines.begin());
        }
    }

    if (!lines.empty()) {
        const std::string &first = *(lines.begin());
        if (lines.back().find_first_not_of(' ') == std::string::npos) {
            lines.pop_back();
        }
    }

    unsigned int commonMargin = std::numeric_limits<unsigned int>::max();
    for (const std::string &l : lines) {
        unsigned int margin = 0;
        for (const char c : l) {
            if (c == ' ') { ++margin; }
            else { break; }
        }
        if (margin < commonMargin) { commonMargin = margin; }
    }

    std::string r;
    for (const std::string &l : lines) {
        if (!r.empty()) { r += '\n'; }
        r += l.substr(commonMargin);
    }

    return r;
}

#endif  // TEST_H
