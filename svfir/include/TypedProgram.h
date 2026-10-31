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

    /// Whether the types of some arguments match this signature. Importantly,
    /// this accounts for varargs.
    bool matches(const std::vector<Type> args) const;
};

//using SymbolTable = std::unordered_map<Id, Type>;

/// Types for names.
class SymbolTable {
    /// Function IDs to their signature (types).
    std::unordered_map<Id, FnSig> fnSigs;
    /// Global variable IDs to their type underneath the pointer.
    std::unordered_map<Id, Type> varTypes;
    /// Function IDs to a map of said function's local variable IDs to their
    /// type.
    std::unordered_map<Id, std::unordered_map<Id, Type>> fnTables;
};

class TypedProgram {
public:
    const Program program;

public:
    TypedProgram(const Program program) : program(program) { }
};

}  // namespace SVFIR

#endif  // TYPEDPROGRAM_H
