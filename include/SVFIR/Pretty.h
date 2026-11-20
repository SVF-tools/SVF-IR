#ifndef PRETTY_H
#define PRETTY_H

#include "SVFIR/Ast.h"
#include "SVFIR/TypedProgram.h"

namespace SVFIR {

/// Return a formatted string version of the given AST node.
std::string pretty(const std::variant<AnyNode, TypedProgram> &);

}  // namespacec SVFIR

#endif  // PRETTY_H
