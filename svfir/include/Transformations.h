#ifndef TRANSFORMATIONS_H
#define TRANSFORMATIONS_H

#include "Ast.h"
#include "TypedProgram.h"

namespace SVFIR {

/// Returns a new program with functions' returns merged into a single return
/// but otherwise equivalent. Functions with only a single return already are
/// untouched. Functions with multiple returns will have a basic block added
/// that has a single return which previous return sites would unconditionally
/// branch to. The return values, if any, will be gathered in a phi statement
/// in this block.
TypedProgram unifyReturns(const TypedProgram &tp);

}  // namespace SVFIR

#endif  // TRANSFORMATIONS_H
