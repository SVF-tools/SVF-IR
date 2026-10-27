#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <optional>

#include "Ast.h"

namespace SVFIR {

bool isRet(const Statement &s);

/// Returns the ID, if any, introduced by a statement. For example,
/// (add %x %y (i32 2)) introduces %x.
std::optional<Id> introducedVar(const Statement &);

/// Returns the IDs, if any, used by a statement. Defined IDs are
/// not considered used. For example, (add %x %y (i32 2)) uses %y.
std::vector<Id> usedVars(const Statement &);

}  // namespace SVFIR

#endif // STATEMENTS_H
