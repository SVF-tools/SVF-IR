#ifndef ERR_H
#define ERR_H

#include <cassert>
#include <optional>
#include <string>
#include <variant>

#include "Result.h"
#include "Span.h"

namespace SVFIR {

struct Err {
    const std::string msg;
    const std::optional<Span> span;

    Err(const std::string msg, const Span span) : msg(msg), span(span) { }
    Err(const std::string msg, const std::optional<Span> span)
    : msg(msg), span(span) { }
    Err(const std::string msg) : msg(msg), span(std::nullopt) { }

    std::string toString(void) const;
};

// Special case of Result with an error.

template <typename T>
bool isErr(const Result<T, Err> res) {
    return std::holds_alternative<Err>(res);
}

template <typename T>
const T getVal(const Result<T, Err> res) {
    assert(std::holds_alternative<T>(res));
    return std::get<T>(res);
}

template <typename T>
const Err getErr(const Result<T, Err> res) {
    assert(isErr(res));
    return std::get<Err>(res);
}

}  // namespace SVFIR

#endif // ERR_H
