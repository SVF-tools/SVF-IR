#ifndef TYPEDPROGRAM_H
#define TYPEDPROGRAM_H

#include <unordered_map>
#include <variant>

#include "Ast.h"

namespace SVFIR {

struct VoidType { };
struct PtrType { };
struct IntType {
    enum class Kind { I8, I16, I32, I64, I128 };
    const Kind kind;
    IntType(const Kind kind) : kind(kind) { }
};
struct FloatType {
    enum class Kind { F16, F32, F64, F128 };
    const Kind kind;
    FloatType(const Kind kind) : kind(kind) { }
};
struct AggType {
    const uint64_t slots;
    AggType(const uint64_t slots) : slots(slots) { }
};
struct TypeId {
    Name name;
    TypeId(const Name name) : name(name) { }
};
struct OpaqueType { };
struct VarargType { };

using Type = std::variant<
    VoidType, PtrType, IntType, FloatType, AggType, TypeId,
    OpaqueType, VarargType
>;

bool isVoidType(const Type &t);
bool isPtrType(const Type &t);
bool isIntType(const Type &t);
bool isFloatType(const Type &t);
bool isAggType(const Type &t);
bool isTypeId(const Type &t);
bool isOpaqueType(const Type &t);
bool isVarargType(const Type &t);

/// Equality:
///  * Any two void types are equal.
///  * Any two pointer types are equal (thus what they may point to is
///    irrelevant).
///  * Any two int types with the same kind are equal.
///  * Any two float types with the same kind are equal.
///  * Any two aggregate types with the same number of slots are equal (thus
///    what those slots may represent is irrelevant).
///  * Any two type IDs with the same ID are equal.
///  * Any two vararg types are equal.
///  * All other pairs of types are not equal (N.B. that includes opaque types).
bool operator==(const Type &, const Type &);
bool operator!=(const Type &, const Type &);

/// Function signature made up of shallow types.
struct FnSig {
    const std::vector<Type> paramTypes;
    const Type retType;
    /// If it accepts a vararg parameter.
    const bool varargParam;

    FnSig(
        const std::vector<Type> paramTypes,
        const Type retType,
        const bool varargParam
    ) : paramTypes(paramTypes), retType(retType), varargParam(varargParam) { }
};

//using SymbolTable = std::unordered_map<Id, Type>;

/// Types for names.
class SymbolTable {
    /// Function IDs to their signature (shallow types).
    std::unordered_map<Id, FnSig> fnSigs;
    /// Global variable IDs to their shallow type underneath the pointer.
    std::unordered_map<Id, Type> varTypes;
    /// Function IDs to a map of said function's local variable IDs to their
    /// shallow type.
    std::unordered_map<Id, std::unordered_map<Id, Type>> fnTables;
    /// Type IDs to the type they represent. Those types are deep, by necessity.
    std::unordered_map<Id, Type> typeAliases;
};

class TypedProgram {
    const Program _program;

public:
    TypedProgram(const Program _program) : _program(_program) { }

    const Program &program(void) const { return _program; }
};

}  // namespace SVFIR

#endif  // TYPEDPROGRAM_H
