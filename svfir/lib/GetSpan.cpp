#include "Ast.h"
#include "Ops.h"
#include "Span.h"

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

Span span(const AnyNode &n) {
    return std::visit(GetSpan(), n);
}

}  // namespace SVFIR
