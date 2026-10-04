#ifndef OPS_H
#define OPS_H

#include "Ast.h"
#include "Span.h"

namespace SVFIR {

    /// Return a formatted string version of the given AST node.
    std::string pretty(const AnyNode &);

    /// Return the Span associated with the given AST node.
    Span span(const AnyNode &);

}  // namespace SVFIR

#endif  // OPS_H
