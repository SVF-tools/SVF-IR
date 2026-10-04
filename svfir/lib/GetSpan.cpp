#include "Ast.h"
#include "Ops.h"
#include "Span.h"

namespace {

using namespace SVFIR;

struct GetSpan {
    // Leaf.
    template <typename T>
    Span operator()(const T &n) { return n.span; }

    // Need to recurse further.
    Span operator()(const Type &n) { return std::visit(GetSpan(), n); }
    Span operator()(const VarId &n) { return std::visit(GetSpan(), n); }
    Span operator()(const Val &n) { return std::visit(GetSpan(), n); }
    Span operator()(const Constant &n) { return std::visit(GetSpan(), n); }
    Span operator()(const PVal &n) { return std::visit(GetSpan(), n); }
    Span operator()(const IVal &n) { return std::visit(GetSpan(), n); }
    Span operator()(const GVal &n) { return std::visit(GetSpan(), n); }
    Span operator()(const Statement &n) { return std::visit(GetSpan(), n); }
    Span operator()(const AnyNode &n) { return std::visit(GetSpan(), n); }
};

};  // Anonymous namespace

namespace SVFIR {

Span span(const AnyNode &n) {
    return std::visit(GetSpan(), n);
}

}  // namespace SVFIR
