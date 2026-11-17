#include <string>
#include <variant>

#include "Ast.h"
#include "Span.h"
#include "SVFIR.h"

namespace {

using namespace SVFIR;

struct GetSpan {
    // Leaf, catch-all.
    template <typename T>
    Span operator()(const T &n) { return n.span; }

    // Need to recurse further.
    template <typename... T>
    Span operator()(const std::variant<T...> &v) {
        return std::visit(GetSpan(), v);
    }
};

};  // Anonymous namespace

namespace SVFIR {

std::string Loc::toString(void) const {
    return "<" + std::to_string(_line) + ", " + std::to_string(_col) + ">";
}

std::string Span::toString(void) const {
    return "[" + start.toString() + ", " + end.toString() + ")";
}

bool Loc::operator==(const Loc &y) const {
    return this->_line == y._line && this->_col == y._col;
}

bool Loc::operator!=(const Loc &y) const {
    return !(*this == y);
}

bool Span::operator==(const Span &s) const {
    return
        this->start == s.start &&
        this->end == s.end &&
        this->gen == s.gen;
}

bool Span::operator!=(const Span &s) const { return !(*this == s); }

Span span(const AnyNode &n) {
    return std::visit(GetSpan(), n);
}

}  // namespace SVFIR
