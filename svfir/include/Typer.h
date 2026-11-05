#ifndef TYPER_H
#define TYPER_H

#include <optional>

#include "Ast.h"
#include "Err.h"
#include "Result.h"
#include "TypedProgram.h"

namespace SVFIR {

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkDuplicateDecs
/// DEPS: --
/// Raise an error if duplicate IDs are declared. This comes down to:
///  * Duplicately named functions and global variables (same namespace).
///  * Duplicately named local variables within a function, including paramters.
///  * Duplicately named blocks within a function.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkDuplicateDecs(const Program &);

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
/// DEPS: checkTerminatingStmts
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
/// PASS: checkUndeclaredBlocks
/// DEPS: --
/// Raise an error if branches are used incorrectly. This comes down to:
///  * br and brif instructions targeting non-existent branches.
///  * phi instructions referring to non-existent blocks in args.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkUndeclaredBlocks(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkPhiStmtArgs
/// DEPS: checkUndeclaredBlocks
/// Raise an error if a phi statement is ill-formed. This comes down to having
/// an argument for each predecessor block of the block the phi statement is in.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkPhiStmtArgs(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkLocalDefBeforeUse
/// DEPS: checkPhiStmtArgs, checkDuplicateDecs
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
/// PASS: checkGlobalDefBeforeUse
/// DEPS: --
/// Raises an error if any global variable is used without being defined.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkGlobalDefBeforeUse(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkConstantTypes
/// DEPS: --
/// Checks that the type of a typed constant matches its partner constant.
/// TODO: this can be pushed to the grammar, consider that...
/// TODO: consider semantics for int/float not fitting in width.
////////////////////////////////////////////////////////////////////////////////
std::optional<Err> checkConstantTypes(const Program &);

////////////////////////////////////////////////////////////////////////////////
/// PASS: buildSymbolTable
/// DEPS: ALL OTHER PASSES
/// A special pass in that it can return something rather than error/no error.
/// Type checks the program, raising an error on any type error. Otherwise
/// returns a symbol table for the program. Type checking involves:
///  * That operands of statements (including types in convert/bitcast)
///    are appropriately typed.
///    * For calls, this involves checking that *some* function exists with
///      a matching signature.
///  * That typed constants carry appropriate types.
////////////////////////////////////////////////////////////////////////////////

}  // namespace SVFIR

#endif  // TYPER_H
