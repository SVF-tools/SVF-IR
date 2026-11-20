#ifndef ASTHELPERS_H
#define ASTHELPERS_H

#include "Ast.h"

namespace SVFIR {

/// Return the Span associated with the given AST node.
Span span(const AnyNode &);

}  // namespace SVFIR

#endif // ASTHELPERS_H
