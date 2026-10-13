#ifndef OPS_H
#define OPS_H

#include "Ast.h"
#include "Err.h"
#include "Result.h"
#include "Span.h"
#include "TypedProgram.h"

namespace SVFIR {

    /// Return a formatted string version of the given AST node.
    std::string pretty(const AnyNode &);

    /// Return the Span associated with the given AST node.
    Span span(const AnyNode &);

    /// Check program for errors and maintain queriable type information.
    Result<TypedProgram, Err> typeProgram(const Program &);

}  // namespace SVFIR

#endif  // OPS_H
