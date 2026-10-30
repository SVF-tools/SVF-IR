#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <optional>

#include "Ast.h"

namespace SVFIR {

/// Returns the variable, if any, introduced by a statement. For example,
/// (add %x %y (i32 2)) introduces %x.
std::optional<Id> introducedVar(const Statement &);

/// Returns the variables, if any, used by a statement. Defined variables are
/// not considered used. For example, (add %x %y (i32 2)) uses %y.
std::vector<Id> usedVars(const Statement &);

/// Returns the values (variables, typed constants), if any, used by a
/// statement. Defined variables are not considered used. For example,
/// (add %x %y (i32 2)) uses %y and (i32 2).
std::vector<Val> usedVals(const Statement &);

/// Return the blocks referred to by a statement, if any. For example,
/// (phi %foo ((%x !1) (%y !x))) uses !1 and !x.
std::vector<Id> usedBlocks(const Statement &);

}  // namespace SVFIR

#endif // STATEMENTS_H
