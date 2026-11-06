#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <optional>

#include "Ast.h"

namespace SVFIR {

/// Returns the variable, if any, introduced by a statement. For example,
/// (add %x %y (i32 2)) introduces %x.
std::optional<Id> introducedVar(const Statement &);

/// Returns the variables, if any, used directly by a statement. This excludes
/// variables referred to in seq constants. Defined variables are not considered
/// used. For example, (add %x %y (i32 2)) uses %y.
std::vector<Id> directlyUsedVars(const Statement &);

/// Returns the values (variables, typed constants), if any, used by a
/// statement. Defined variables are not considered used. For example,
/// (add %x %y (i32 2)) uses %y and (i32 2).
/// NOTE: integers where IVal occurs and nulls where PVal occurs are ignored.
std::vector<Val> usedVals(const Statement &);

/// Return the blocks referred to by a statement, if any. For example,
/// (phi %foo ((%x !1) (%y !x))) uses !1 and !x.
std::vector<Id> usedBlocks(const Statement &);

/// Whether a statement is one which can (and can only) terminate a basic
/// block. These are 'ret', 'br', and 'brif'.
bool isTerminatingStmt(const Statement &);

}  // namespace SVFIR

#endif // STATEMENTS_H
