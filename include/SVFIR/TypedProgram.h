#ifndef TYPEDPROGRAM_H
#define TYPEDPROGRAM_H

#include <unordered_map>
#include <variant>

#include "SVFIR/Ast.h"

namespace SVFIR {

/// Check program for any semantic errors including name misuses, type errors,
/// and others, and produce queriable type information.
Result<struct TypedProgram, Err> typeProgram(const Program &);

/// Function signature made up of types.
struct FnSig {
    const std::vector<Type> params;
    const Type ret;
    /// If it accepts a vararg parameter.
    const bool vararg;

    FnSig(const Function &fn);

    /// Whether some expected return type and the types of some arguments
    /// match this signature. Importantly, this accounts for varargs.
    bool matches(const std::vector<Type> &args, const Type &expRet) const;

    /// Exact equality of all fields.
    bool operator==(const FnSig &) const;
    bool operator!=(const FnSig &) const;
};

/// Types for names.
struct SymbolTable {
    /// Variables to types.
    using TypeMap = std::unordered_map<Name, Type>;
    /// Function to signatures.
    using SigMap = std::unordered_map<Name, FnSig>;
    /// Function to (variables to types).
    using FnTables = std::unordered_map<Name, TypeMap>;

    /// Function names to their signature (types).
    const SigMap fnSigs;
    /// Function names to a map of said function's local variable names to their
    /// type.
    const FnTables fnTables;

    SymbolTable(const SigMap &fnSigs, const FnTables fnTables)
    : fnSigs(fnSigs), fnTables(fnTables) { }
};

/// A valid program and type information about it.
/// TODO: technically this type isn't actually guaranteeing anything as it can
///       be constructed with any program and any symbol table, so it's doing
///       more of a documentation and encapsulation thing than enforcing
///       contracts. To do type checking in the constructor would make things
///       more expensive and disallow us from e.g. making small changes to a
///       symbol table after a transformation. Something to think about, maybe
///       make the constructor private and give friend access? That's not
///       extensible.
class TypedProgram {
public:
    const Program program;
    const SymbolTable symbols;

public:
    /// *Program must be a valid program and symbols must correctly represent
    /// the type information of program.*
    TypedProgram(const Program &program, const SymbolTable &symbols)
    : program(program), symbols(symbols) { }
};

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
/// DEPS: checkConstantTypes, all passes in Validation.h.
/// Type checks the program, raising an error on any type error. Otherwise
/// returns a symbol table for the program. Type checking involves:
///  * That operands of statements (including types in convert/bitcast)
///    are appropriately typed.
///    * For calls, this involves checking that *some* function exists with
///      a matching signature.
///  * That typed constants carry appropriate types.
////////////////////////////////////////////////////////////////////////////////
Result<SymbolTable, Err> buildSymbolTable(const Program &);

}  // namespace SVFIR

#endif  // TYPEDPROGRAM_H
