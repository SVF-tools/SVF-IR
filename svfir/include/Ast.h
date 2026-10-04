#ifndef AST_H
#define AST_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "SExprs.h"
#include "Version.h"

namespace SVFIR {

using MaybeMetadata = std::optional<struct Metadata>;

struct Metadata final {
    const SExprs::SExpr data;

    Metadata(const SExprs::SExpr data) : data(data) { }
};

/// <scalar-type> (1)
struct VoidType final { };

/// <scalar-type> (2)
struct IntType final {
    enum class Kind { I8, I16, I32, I64, I128 };

    const Kind kind;

    IntType(const Kind kind) : kind(kind) { }
};

/// <scalar-type> (3)
struct FloatType final {
    enum class Kind { F16, F32, F64, F128 };

    const Kind kind;

    FloatType(const Kind kind) : kind(kind) { }
};

/// <scalar-type> (4)
struct PtrType final { };

/// <scalar-type> (5)
struct BoolType final { };

/// <seq-type>
struct AggType final {
    const uint64_t slots;

    AggType(const uint64_t slots) : slots(slots) { }
};

/// <tid>
struct TypeId final {
    const std::string id;

    TypeId(const std::string id) : id(id) { }
};

/// <type>
using Type = std::variant<
    VoidType, IntType, FloatType, PtrType, BoolType, AggType, TypeId
>;

/// <lid>
struct LocalId final {
    const std::string id;

    LocalId(const std::string id) : id(id) { }
};

/// <gid>
struct GlobalId final {
    const std::string id;

    GlobalId(const std::string id) : id(id) { }
};

/// <var-id>
using VarId = std::variant<LocalId, GlobalId>;

/// <bid>
struct BlockId final {
    const std::string id;

    BlockId(const std::string id) : id(id) { }
};

/// <typed-lid>
struct TypedId final {
    const LocalId id;
    const Type type;

    TypedId(const LocalId id, const Type type) : id(id), type(type) { }
};

/// <int-const>
struct IntConstant final {
    // TODO: this should be an arbitrary precision integer.
    const std::string val;

    IntConstant(const std::string val) : val(val) { }
};

/// <flt-const>
struct FloatConstant final {
    static_assert(std::numeric_limits<double>::is_iec559);
    // TODO: this needs to be a real value.
    const std::string val;

    FloatConstant(const std::string val) : val(val) { }
};

/// <null-const>
struct NullConstant final {
    NullConstant(void) { }
};

/// <bool-const>
struct BoolConstant final {
    const bool val;

    BoolConstant(const bool val) : val(val) { }
};

/// <val>
using Val = std::variant<VarId, struct TypedConstant>;

/// <seq-const>
struct SeqConstant final {
    const std::vector<Val> vals;

    SeqConstant(const std::vector<Val> vals) : vals(vals) { }
};

/// <const>
using Constant = std::variant<
    IntConstant, FloatConstant, NullConstant, BoolConstant, SeqConstant
>;

/// <typed-const>
struct TypedConstant {
    const Constant constant;
    const Type type;

    TypedConstant(const Constant constant, const Type type)
    : constant(constant), type(type) { }
};

/// <pval>
using PVal = std::variant<VarId, NullConstant>;

/// <ival>
using IVal = std::variant<VarId, IntConstant>;

/// <gval>
using GVal = std::variant<GlobalId, TypedConstant>;

/// <preamble>
struct Preamble final {
    const Version version;
    const std::string source;
    const MaybeMetadata md;

    Preamble(
        const Version version,
        const std::string source,
        const MaybeMetadata md
    ) : version(version), source(source), md(md) { }
};

/// <variable>
struct Variable final {
    const GlobalId id;
    // A missing value indicates a declaration, otherwise it's a definition.
    const std::optional<GVal> val;
    const MaybeMetadata md;

    Variable(
        const GlobalId id,
        const std::optional<GVal> val,
        const MaybeMetadata md
    ) : id(id), val(val), md(md) { }

    bool isOpaque(void) const;
};

/// <param>
struct Param final {
    const LocalId id;
    const Type type;
    const MaybeMetadata md;

    Param(const LocalId id, const Type type, const MaybeMetadata md)
    : id(id), type(type), md(md) { }
};

/// <function>
struct Function final {
    const GlobalId id;
    // The parameters are stored across two fields:
    // 1. all ordinary parameters, and
    const std::vector<Param> params;
    // 2. the optional final vararg parameter.
    const std::optional<LocalId> varargParam;
    const Type type;
    const std::optional<std::vector<struct BasicBlock>> body;
    const MaybeMetadata md;

    Function(
        const GlobalId id,
        const std::vector<Param> params,
        const std::optional<LocalId> varargParam,
        const Type type,
        const std::optional<std::vector<struct BasicBlock>> body,
        const MaybeMetadata md
    ) :
        id(id),
        params(params),
        varargParam(varargParam),
        type(type),
        body(body),
        md(md)
    { }

    bool isOpaque(void) const;
};

/// <type-alias>
struct TypeAlias final {
    const TypeId id;
    // A missing value indicates a declaration, otherwise it's a definition.
    const std::optional<Type> type;
    const MaybeMetadata md;

    TypeAlias(
        const TypeId id,
        const std::optional<Type> type,
        const MaybeMetadata md
    ) : id(id), type(type), md(md) { }

    bool isOpaque(void) const;
};

/// <program>
struct Program final {
    const Preamble preamble;
    const std::vector<TypeAlias> types;
    const std::vector<Variable> variables;
    const std::vector<Function> functions;
    const MaybeMetadata md;

    Program(
        const Preamble preamble,
        const std::vector<TypeAlias> types,
        const std::vector<Variable> variables,
        const std::vector<Function> functions,
        const MaybeMetadata md
    ) :
        preamble(preamble),
        types(types),
        variables(variables),
        functions(functions),
        md(md)
    { }
};

/// <control-inst> (1)/phi
struct PhiStmt final {
    struct Operand {
        const Val val;
        const BlockId definingBlock;
        Operand(const Val val, const BlockId definingBlock)
        : val(val), definingBlock(definingBlock) { }
    };

    const VarId id;
    // ! Guaranteed non-zero length.
    const std::vector<Operand> args;
    const MaybeMetadata md;

    PhiStmt(
        const VarId id,
        const std::vector<Operand> args,
        const MaybeMetadata md
    ) : id(id), args(args), md(md) { }
};

/// <control-inst> (2)/call
struct CallStmt final {
    const std::optional<LocalId> id;
    const PVal callee;
    const std::vector<Val> args;
    const MaybeMetadata md;

    CallStmt(
        const std::optional<LocalId> id,
        const PVal callee,
        const std::vector<Val> args,
        const MaybeMetadata md
    ) : id(id), callee(callee), args(args), md(md) { }
};

/// <control-inst> (3)/jump
struct BrStmt final {
    const BlockId target;
    const MaybeMetadata md;

    BrStmt(const BlockId target, const MaybeMetadata md)
    : target(target), md(md) { }
};

/// <control-inst> (4)/jumpif
struct BrifStmt final {
    const Val cond;
    const BlockId ifTarget;
    const BlockId elseTarget;
    const MaybeMetadata md;

    BrifStmt(
        const Val cond,
        const BlockId ifTarget,
        const BlockId elseTarget,
        const MaybeMetadata md
    ) : cond(cond), ifTarget(ifTarget), elseTarget(elseTarget), md(md) { }
};

/// <control-inst> (5)/return
struct RetStmt final {
    const std::optional<Val> val;
    const MaybeMetadata md;

    RetStmt(const std::optional<Val> val, const MaybeMetadata md)
    : val(val), md(md) { }
};

/// <cmp-inst> with <cmp-op> through op.
struct CmpStmt final {
    enum class Operator { LT, LE, GT, GE, EQ, NEQ };

    const LocalId id;
    const Operator op;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    CmpStmt(
        const LocalId id,
        const Operator op,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), op(op), left(left), right(right), md(md) { }
};

/// <mem-inst> (1)
struct AllocStmt final {
    enum class Kind { HEAP, STACK };

    const Kind kind;
    const LocalId id;
    const MaybeMetadata md;

    AllocStmt(const Kind kind, const LocalId id, const MaybeMetadata md)
    : kind(kind), id(id), md(md) { }
};

/// <mem-inst> (2)
struct StoreStmt final {
    const Val val;
    const PVal dst;
    const MaybeMetadata md;

    StoreStmt(const Val val, const PVal dst, const MaybeMetadata md)
    : val(val), dst(dst), md(md) { }
};

/// <mem-inst> (3)
struct LoadStmt final {
    const TypedId tid;
    const PVal src;
    const MaybeMetadata md;

    LoadStmt(const TypedId tid, const PVal src, const MaybeMetadata md)
    : tid(tid), src(src), md(md) { }
};

/// <mem-inst> (4)
struct FieldStmt final {
    const LocalId id;
    const PVal src;
    const IVal index;
    const MaybeMetadata md;

    FieldStmt(
        const LocalId id,
        const PVal src,
        const IVal index,
        const MaybeMetadata md
    ) : id(id), src(src), index(index), md(md)
    { }
};

/// <arith-inst> (1)
struct AddStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    AddStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <arith-inst> (2)
struct SubStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    SubStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <arith-inst> (3)
struct MulStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    MulStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <arith-inst> (4)
struct DivStmt final {
    enum class Kind { UNSIGNED, SIGNED };

    const Kind kind;
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    DivStmt(
        const Kind kind,
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : kind(kind), id(id), left(left), right(right), md(md)
    { }
};

/// <arith-inst> (5)
struct RemStmt final {
    enum class Kind { UNSIGNED, SIGNED };

    const Kind kind;
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    RemStmt(
        const Kind kind,
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : kind(kind), id(id), left(left), right(right), md(md)
    { }
};

/// <bit-inst> (1)
struct NotStmt final {
    const LocalId id;
    const Val val;
    const MaybeMetadata md;

    NotStmt(const LocalId id, const Val val, const MaybeMetadata md)
    : id(id), val(val), md(md) { }
};

/// <bit-inst> (2)
struct AndStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    AndStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <bit-inst> (3)
struct OrStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    OrStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <bit-inst> (4)
struct XorStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    XorStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <bit-inst> (5)
struct ShiftlStmt final {
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    ShiftlStmt(
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : id(id), left(left), right(right), md(md)
    { }
};

/// <bit-inst> (6)
struct ShiftrStmt final {
    enum class Kind { LOGICAL, ARITHMETIC };

    const Kind kind;
    const LocalId id;
    const Val left;
    const Val right;
    const MaybeMetadata md;

    ShiftrStmt(
        const Kind kind,
        const LocalId id,
        const Val left,
        const Val right,
        const MaybeMetadata md
    ) : kind(kind), id(id), left(left), right(right), md(md)
    { }
};

/// <misc-inst> (1)
struct AssignStmt final {
    const LocalId id;
    const Val val;
    const MaybeMetadata md;

    AssignStmt(const LocalId id, const Val val, const MaybeMetadata md)
    : id(id), val(val), md(md) { }
};

/// <misc-inst> (2)
struct VarargStmt final {
    const TypedId tid;
    const IVal index;
    const MaybeMetadata md;

    VarargStmt(const TypedId tid, const IVal index, const MaybeMetadata md)
    : tid(tid), index(index), md(md) { }
};

/// <misc-inst> (3)
struct BlackholeStmt final {
    const TypedId tid;
    const MaybeMetadata md;

    BlackholeStmt(const TypedId tid, const MaybeMetadata md)
    : tid(tid), md(md) { }
};

/// <instruction>
using Statement = std::variant<
    PhiStmt, CallStmt, BrStmt, BrifStmt, RetStmt,
    CmpStmt,
    AllocStmt, StoreStmt, LoadStmt, FieldStmt,
    // TODO conversions...
    AddStmt, SubStmt, MulStmt, DivStmt, RemStmt,
    NotStmt, AndStmt, OrStmt, XorStmt, ShiftlStmt, ShiftrStmt,
    AssignStmt, VarargStmt, BlackholeStmt
>;

/// <basic-block>
struct BasicBlock final {
    const BlockId id;
    const std::vector<Statement> stmts;
    const MaybeMetadata md;

    BasicBlock(
        const BlockId id,
        const std::vector<Statement> stmts,
        const MaybeMetadata md
    ) : id(id), stmts(stmts), md(md) { }
};

/// Represents all AST nodes, including any possible field of an AST node
/// (notably, that includes variants of leaf AST nodes like Statement).
/// This allows for easy operations that are defined for each type like
/// pretty printing.
using AnyNode = std::variant<
    Metadata,
    VoidType, IntType, FloatType, PtrType, BoolType, AggType, TypeId,
    LocalId, GlobalId, BlockId, TypedId,
    IntConstant, FloatConstant, NullConstant, BoolConstant, SeqConstant,
    TypedConstant,
    Preamble, Variable, Param, Function, TypeAlias, Program,
    PhiStmt, CallStmt, BrStmt, BrifStmt, RetStmt, CmpStmt, AllocStmt, StoreStmt,
    LoadStmt, FieldStmt, AddStmt, SubStmt, MulStmt, DivStmt, RemStmt, NotStmt,
    AndStmt, OrStmt, XorStmt, ShiftlStmt, ShiftrStmt, AssignStmt, VarargStmt,
    BlackholeStmt, BasicBlock,
    Type, VarId, Constant, Val, PVal, IVal, GVal, Statement
>;

}  // namespace SVFIR

#endif  // AST_H
