#ifndef UNIFYRETURNS_H
#define UNIFYRETURNS_H

namespace SVFIR {

/// Returns a new program with functions' returns merged into a single return
/// but otherwise equivalent. Functions with only a single return already are
/// untouched. Functions with multiple returns will have a basic block added
/// that has a single return which previous return sites would unconditionally
/// branch to. The return values, if any, will be gathered in a phi statement
/// in this block.
Program unifyReturns(const Program &p);

}  // namespace SVFIR

#endif  // UNIFYRETURNS_H
