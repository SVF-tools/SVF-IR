#ifndef TYPEDPROGRAM_H
#define TYPEDPROGRAM_H

#include <unordered_map>
#include <variant>

#include "Ast.h"

namespace SVFIR {

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
    /// Global variable (non-function) names to their type underneath the
    /// pointer. Opaque global variables are given void types here.
    TypeMap globalVarTypes;
    /// Function names to a map of said function's local variable names to their
    /// type.
    const FnTables fnTables;

    SymbolTable(
        const SigMap &fnSigs,
        const TypeMap &globalVarTypes,
        const FnTables fnTables
    ) : fnSigs(fnSigs), globalVarTypes(globalVarTypes), fnTables(fnTables) { }
};

class TypedProgram {
public:
    const Program program;
    const SymbolTable symbols;

public:
    TypedProgram(const Program &program, const SymbolTable &symbols)
    : program(program), symbols(symbols) { }
};

}  // namespace SVFIR

#endif  // TYPEDPROGRAM_H
