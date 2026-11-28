#ifndef ERR_H
#define ERR_H

#include <optional>
#include <string>
#include <variant>

#include "SVFIR/Contracts.h"
#include "SVFIR/Span.h"
#include "SVFIR/Variant.h"

namespace SVFIR {

struct Err {
    const std::string msg;
    const std::optional<Span> span;

    Err(const std::string &msg, const Span &span) : msg(msg), span(span) { }
    Err(const std::string &msg, const std::optional<Span> &span)
    : msg(msg), span(span) { }
    Err(const std::string &msg) : msg(msg), span(std::nullopt) { }

    bool operator==(const Err &e) const;

    std::string toString(void) const;
};

/// Success or error.
template <typename T>
using Result = std::variant<T, Err>;

}  // namespace SVFIR

#endif // ERR_H
