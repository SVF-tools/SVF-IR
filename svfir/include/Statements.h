#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <optional>

#include "Ast.h"

namespace SVFIR {

bool isRet(const Statement &s);

/// Returns the ID, if any, introduced by a statement. For example,
/// (add %x %y (i32 2)) introduces %x.
std::optional<Id> introducedId(const Statement &);

}  // namespace SVFIR

#endif // STATEMENTS_H
