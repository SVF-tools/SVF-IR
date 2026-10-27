#ifndef ERR_H
#define ERR_H

#include <optional>
#include <string>
#include <variant>

#include "Contracts.h"
#include "Result.h"
#include "Span.h"
#include "Variant.h"

namespace SVFIR {

struct Err {
    const std::string msg;
    const std::optional<Span> span;

    Err(const std::string &msg, const Span &span) : msg(msg), span(span) { }
    Err(const std::string &msg, const std::optional<Span> &span)
    : msg(msg), span(span) { }
    Err(const std::string &msg) : msg(msg), span(std::nullopt) { }

    std::string toString(void) const;
};

// Special case of Result with an error.

template <typename T>
const T &getVal(const Result<T, Err> &res) {
    insist(is<T>(res));
    return get<T>(res);
}

}  // namespace SVFIR

#endif // ERR_H
