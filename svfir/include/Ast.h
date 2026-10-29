#ifndef AST_H
#define AST_H

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Contracts.h"
#include "SExprs.h"
#include "Version.h"

namespace SVFIR {

using Name = std::string;

using MaybeMetadata = std::optional<struct Metadata>;

struct Metadata final {
    const SExprs::SExpr data;
    const Span span;

    Metadata(const SExprs::SExpr &data, const Span &span)
    : span(span), data(data) { }
};

/// <lid>/<gid>/<bid>/<tid>/<var-id>
struct Id final {
    const Name name;
    const Span span;

    Id(const Name &name, const Span &span) : span(span), name(name) {
        insist(name.size() >= 2);
        insist(name[0] == '%' || name[0] == '@' || name[0] == '!');
    }

    /// One of the following predicate functions is guaranteed to be true
    /// and the remainder false.
    //
    bool isLocal(void) const;
    bool isGlobal(void) const;
    bool isBlock(void) const;
};

/// <scalar-type> (1)
struct VoidDecType final {
    const Span span;

    VoidDecType(const Span &span) : span(span) { }
};

/// <scalar-type> (2)
struct IntDecType final {
    enum class Kind { I8, I16, I32, I64, I128 };

    const Kind kind;
    const Span span;

    IntDecType(const Kind &kind, const Span &span)
    : span(span), kind(kind) { }
};

/// <scalar-type> (3)
struct FloatDecType final  {
    enum class Kind { F16, F32, F64, F128 };

    const Kind kind;
    const Span span;

    FloatDecType(const Kind &kind, const Span &span)
    : span(span), kind(kind) { }
};

/// <scalar-type> (4)
struct PtrDecType final {
    const Span span;

    PtrDecType(const Span &span) : span(span) { }
};

/// <seq-type>
struct AggDecType final {
    const uint64_t slots;
    const Span span;

    AggDecType(const uint64_t &slots, const Span &span)
    : span(span), slots(slots) { }
};

/// <type>
using DecType = std::variant<
    VoidDecType, IntDecType, FloatDecType, PtrDecType, AggDecType, Id
>;

/// <typed-lid>
struct TypedId final {
    const Id id;
    const DecType type;
    const Span span;

    TypedId(const Id &id, const DecType &type, const Span &span)
    : span(span), id(id), type(type) { }
};

/// <int-const>
struct IntConstant final {
    // TODO: this should be an arbitrary precision integer.
    const std::string val;
    const Span span;

    IntConstant(const std::string &val, const Span &span)
    : span(span), val(val) { }
};

/// <flt-const>
struct FloatConstant final {
    static_assert(std::numeric_limits<double>::is_iec559);
    // TODO: this needs to be a real value.
    const std::string val;
    const Span span;

    FloatConstant(const std::string &val, const Span &span)
    : span(span), val(val) { }
};

/// <null-const>
struct NullConstant final {
    const Span span;

    NullConstant(const Span &span) : span(span) { }
};

/// <val>/<gval>
using Val = std::variant<Id, struct TypedConstant>;

/// <seq-const>
struct SeqConstant final {
    const std::vector<Val> vals;
    const Span span;

    SeqConstant(const std::vector<Val> &vals, const Span &span)
    : span(span), vals(vals) { }
};

/// <const>
using Constant = std::variant<
    IntConstant, FloatConstant, NullConstant, SeqConstant
>;

/// <typed-const>
struct TypedConstant final {
    const Constant constant;
    const DecType type;
    const MaybeMetadata md;
    const Span span;

    TypedConstant(
        const Constant &constant,
        const DecType &type,
        const MaybeMetadata md,
        const Span &span
    ) : span(span), constant(constant), type(type), md(md) { }
};

/// <pval>
using PVal = std::variant<Id, NullConstant>;

/// <ival>
using IVal = std::variant<Id, IntConstant>;

/// <preamble>
struct Preamble final {
    const Version version;
    const std::string source;
    const Span span;

    Preamble(
        const Version &version,
        const std::string &source,
        const Span &span
    ) : span(span), version(version), source(source) { }
};

/// <variable>
struct GlobalVar final {
    const Id id;
    // A missing value indicates a declaration, otherwise it's a definition.
    const std::optional<Val> val;
    const MaybeMetadata md;
    const Span span;

    GlobalVar(
        const Id &id,
        const std::optional<Val> &val,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), val(val), md(md) { }

    bool isOpaque(void) const;
};

/// <param>
struct Param final {
    const Id id;
    const DecType type;
    const MaybeMetadata md;
    const Span span;

    Param(
        const Id &id,
        const DecType &type,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), type(type), md(md) { }
};

using FunctionBody = std::vector<struct BasicBlock>;
/// <function>
struct Function final {
    const Id id;
    const std::vector<Param> params;
    const bool vararg;
    const DecType type;
    const std::optional<FunctionBody> body;
    const MaybeMetadata md;
    const Span span;

    Function(
        const Id &id,
        const std::vector<Param> &params,
        const bool &vararg,
        const DecType &type,
        const std::optional<FunctionBody> &body,
        const MaybeMetadata &md,
        const Span &span
    ) :
        span(span),
        id(id),
        params(params),
        vararg(vararg),
        type(type),
        body(body),
        md(md)
    { }

    Function(const Function &fn, const FunctionBody &body)
    :
        span(fn.span),
        id(fn.id),
        params(fn.params),
        vararg(fn.vararg),
        type(fn.type),
        body(body),
        md(fn.md)
    { }

    bool isOpaque(void) const;
};

/// <program>
struct Program final {
    const Preamble preamble;
    const std::vector<GlobalVar> globalVars;
    const std::vector<Function> functions;
    const MaybeMetadata md;
    const Span span;

    Program(
        const Preamble &preamble,
        const std::vector<GlobalVar> &globalVars,
        const std::vector<Function> &functions,
        const MaybeMetadata &md,
        const Span &span
    ) :
        span(span),
        preamble(preamble),
        globalVars(globalVars),
        functions(functions),
        md(md)
    { }
};

/// <control-inst> (1)/phi
struct PhiStmt final {
    struct Arg {
        const Val val;
        const Id pred;
        const Span &span;
        Arg(const Val &val, const Id &pred, const Span span)
        : val(val), pred(pred), span(span) { }
    };

    const Id id;
    // ! Guaranteed non-zero length.
    const std::vector<Arg> args;
    const MaybeMetadata md;
    const Span span;

    PhiStmt(
        const Id &id,
        const std::vector<Arg> &args,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), args(args), md(md) { }
};

/// <control-inst> (2)/call
struct CallStmt final {
    const std::optional<Id> id;
    const PVal callee;
    const std::vector<Val> args;
    const MaybeMetadata md;
    const Span span;

    CallStmt(
        const std::optional<Id> &id,
        const PVal &callee,
        const std::vector<Val> &args,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), callee(callee), args(args), md(md) { }
};

/// <control-inst> (3)/jump
struct BrStmt final {
    const Id target;
    const MaybeMetadata md;
    const Span span;

    BrStmt(const Id &target, const MaybeMetadata &md, const Span &span)
    : span(span), target(target), md(md) { }
};

/// <control-inst> (4)/jumpif
struct BrifStmt final {
    const Val cond;
    const Id ifTarget;
    const Id elseTarget;
    const MaybeMetadata md;
    const Span span;

    BrifStmt(
        const Val &cond,
        const Id &ifTarget,
        const Id &elseTarget,
        const MaybeMetadata &md,
        const Span &span
    ) :
        span(span),
        cond(cond),
        ifTarget(ifTarget),
        elseTarget(elseTarget),
        md(md)
    { }
};

/// <control-inst> (5)/return
struct RetStmt final {
    // No val indicates void.
    const std::optional<Val> val;
    const MaybeMetadata md;
    const Span span;

    RetStmt(
        const std::optional<Val> &val,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), val(val), md(md) { }
};

/// <mem-inst> (1)
struct AllocStmt final {
    enum class Kind { HEAP, STACK };

    const Kind kind;
    const Id id;
    const MaybeMetadata md;
    const Span span;

    AllocStmt(
        const Kind &kind,
        const Id &id,
        const MaybeMetadata &md,
        const Span &span)
    : span(span), kind(kind), id(id), md(md) { }
};

/// <mem-inst> (2)
struct StoreStmt final {
    const Val val;
    const PVal dst;
    const MaybeMetadata md;
    const Span span;

    StoreStmt(
        const Val &val,
        const PVal &dst,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), val(val), dst(dst), md(md) { }
};

/// <mem-inst> (3)
struct LoadStmt final {
    const TypedId tid;
    const PVal src;
    const MaybeMetadata md;
    const Span span;

    LoadStmt(
        const TypedId &tid,
        const PVal &src,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), tid(tid), src(src), md(md) { }
};

/// <mem-inst> (4)
struct FieldStmt final {
    const Id id;
    const PVal src;
    const IVal index;
    const MaybeMetadata md;
    const Span span;

    FieldStmt(
        const Id &id,
        const PVal &src,
        const IVal &index,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), src(src), index(index), md(md) { }
};

/// <conversion-stmt> (1)
struct BitCastStmt final {
    const Id id;
    const Val val;
    const DecType type;
    const MaybeMetadata md;
    const Span span;

    BitCastStmt(
        const Id &id,
        const Val &val,
        const DecType &type,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), val(val), type(type), md(md) { }
};

/// <conversion-stmt> (2)
struct ConvertStmt final {
    enum class ExtKind { ZERO, SIGN };

    const Id id;
    const Val val;
    const DecType type;
    std::optional<ExtKind> extKind;
    const MaybeMetadata md;
    const Span span;

    ConvertStmt(
        const Id &id,
        const Val &val,
        const DecType &type,
        std::optional<ExtKind> extKind,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), val(val), type(type), md(md) { }
};

/// <arith-inst>/<bit-stmt>/<cmp-inst>, <cmp-op> through op.
/// <cmp-inst> with <cmp-op> through op.
struct BinaryOpStmt final {
    enum class Op {
        ADD, SUB, MUL, UDIV, SDIV, UREM, SREM,
        AND, OR, XOR, SHIFTL, LSHIFTR, ASHIFTR,
        LT, LE, GT, GE, EQ, NEQ
    };

    const Op op;
    const Id id;
    const Val left;
    const Val right;
    const MaybeMetadata md;
    const Span span;

    BinaryOpStmt(
        const Op &op,
        const Id &id,
        const Val &left,
        const Val &right,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), op(op), id(id), left(left), right(right), md(md) { }
};

struct UnaryOpStmt final {
    enum class Op { NOT };

    const Op op;
    const Id id;
    const Val val;
    const MaybeMetadata md;
    const Span span;

    UnaryOpStmt(
        const Op &op,
        const Id &id,
        const Val &val,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), op(op), id(id), val(val), md(md) { }
};

/// <misc-inst> (1)
struct VarargStmt final {
    const TypedId tid;
    const IVal index;
    const MaybeMetadata md;
    const Span span;

    VarargStmt(
        const TypedId &tid,
        const IVal &index,
        const MaybeMetadata &md,
        const Span &span
    )
    : span(span), tid(tid), index(index), md(md) { }
};

/// <misc-inst> (2)
struct BlackholeStmt final {
    // To be impure is to be effectful.
    const bool pure;
    const std::optional<TypedId> tid;
    const MaybeMetadata md;
    const Span span;

    BlackholeStmt(
        const bool &pure,
        const std::optional<TypedId> &tid,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), pure(pure), tid(tid), md(md) { }
};

/// <instruction>
using Statement = std::variant<
    PhiStmt, CallStmt, BrStmt, BrifStmt, RetStmt,
    AllocStmt, StoreStmt, LoadStmt, FieldStmt,
    BitCastStmt, ConvertStmt,
    BinaryOpStmt, UnaryOpStmt,
    VarargStmt, BlackholeStmt
>;

/// <basic-block>
struct BasicBlock final {
    const Id id;
    const std::vector<Statement> stmts;
    const MaybeMetadata md;
    const Span span;

    BasicBlock(
        const Id &id,
        const std::vector<Statement> &stmts,
        const MaybeMetadata &md,
        const Span &span
    ) : span(span), id(id), stmts(stmts), md(md) {
        insist(!empty(stmts));
    }
};

/// Represents all AST nodes, including any possible field of an AST node
/// (notably, that includes variants of leaf AST nodes like Statement).
/// This allows for easy operations that are defined for each type like
/// pretty printing.
using AnyNode = std::variant<
    Metadata,
    Id,
    VoidDecType, IntDecType, FloatDecType, PtrDecType, AggDecType,
    IntConstant, FloatConstant, NullConstant, SeqConstant,
    TypedId, TypedConstant,
    Preamble, GlobalVar, Param, Function, Program,
    PhiStmt, CallStmt, BrStmt, BrifStmt, RetStmt, AllocStmt, StoreStmt,
    LoadStmt, FieldStmt, BitCastStmt, ConvertStmt,
    BinaryOpStmt, UnaryOpStmt, BlackholeStmt, BasicBlock,
    DecType, Constant, Val, PVal, IVal, Statement
>;

}  // namespace SVFIR

#endif  // AST_H
