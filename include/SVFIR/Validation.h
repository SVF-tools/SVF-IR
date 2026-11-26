#ifndef VALIDATION_H
#define VALIDATION_H

#include <functional>
#include <optional>

#include "SVFIR/Ast.h"
#include "SVFIR/Err.h"
#include "SVFIR/Result.h"
#include "SVFIR/TypedProgram.h"

namespace SVFIR {

/// An analysis that just checks things. Produces no result or tranformation.
using CheckAnalysis = std::function<std::optional<Err>(const Program &)>;

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkDuplicateVars
/// DEPS: --
/// Raise an error if duplicate variable IDs are declared. This comes down to:
///  * Duplicately named functions and global variables (same namespace).
///  * Duplicately named local variables within a function, including paramters.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkDuplicateVars(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkDuplicateBlocks
/// DEPS: --
/// Raise an error if duplicate block IDs are declared.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkDuplicateBlocks(const Program &);

///////////////////////////////////////////////////////////////////////////////
/// PASS: checkUndeclaredBlocks
/// DEPS: --
/// Raise an error if branches are used incorrectly. This comes down to:
///  * br and brif instructions targeting non-existent branches.
///  * phi instructions referring to non-existent blocks in args.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkUndeclaredBlocks(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkTerminatingStmts
/// DEPS: --
/// Raise an error if either:
///  * A basic block ends with a non-terminating statement.
///  * A terminating statement appears anywhere but as the last statement of a
///    basic block.
///
/// Terminating statements are br, brif, and ret.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkTerminatingStmts(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkEntryPredecessors
/// DEPS: checkDuplicateBlocks, checkTerminatingStmts
/// Raise an error if the entry block has a predecessor.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkEntryPredecessors(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkVarargStmts
/// DEPS: --
/// Raise an error if a vararg statement is used in a non-vararg function.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkVarargStmts(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkPhiStmtArgs
/// DEPS: checkDupicateVars, checkDuplicateBlocks, checkUndeclaredBlocks
/// Raise an error if a phi statement is ill-formed. This comes down to having
/// (and only having) an argument for each predecessor block of the block the
/// phi statement is in.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkPhiStmtArgs(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkLocalDefBeforeUse
/// DEPS: checkPhiStmtArgs, checkUndeclaredBlocks, checkDuplicateBlocks,
///       checkDuplicateVars
/// Raises an error if any local variable is used before being appropriately
/// defined. Our programs must be in SSA form, so this means:
///  * For non-phi statements, any use of a local variable must be dominated by
///    the definition of said variable.
///  * For phi statements, given an argument like (%var, !pred), the definition
///    of %var must dominate !pred.
/// Dependencies allow us to assume we will not come across two definitions of
/// the same variable, phi statements arguments refer to predecessors validly.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkLocalDefBeforeUse(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkUndeclaredGlobals
/// DEPS: --
/// Raises an error if any global variable is used without being declared.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkUndeclaredGlobals(const Program &);

/// Runs the analyses provided, in the order provided, against the provided
/// program. If an analysis fails, immediately returns the error (i.e., short
/// circuits).
std::optional<Err> analyse(const Program &, const std::vector<CheckAnalysis> &);

}  // namespace SVFIR

#endif  // VALIDATION_H
