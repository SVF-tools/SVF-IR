#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Analyses.h"
#include "Ast.h"
#include "Contracts.h"
#include "ControlFlowGraph.h"
#include "DominatorTree.h"
#include "Err.h"
#include "Index.h"
#include "Result.h"
#include "Statements.h"
#include "SVFIR.h"
#include "TypedProgram.h"
#include "Variant.h"

namespace {

using namespace SVFIR;

const PtrType FAB_PTR_TYPE = PtrType(FAB_SPAN);
const IntType FAB_INT8_TYPE = IntType(IntType::Kind::I8, FAB_SPAN);
const VoidType FAB_VOID_TYPE = VoidType(FAB_SPAN);

// buildSymbolTable ////////////////////////////////////////////////////////////
/// To be used by the buildSymbolTable pass. (*this)() should be cotinually
/// called on statements in def-use order. At the end of this, a mapping of
/// local names to types can then be pulled out via types().
struct Typer {
private:
    SymbolTable::TypeMap _types;
    /// Function we're typing.
    const Function &fn;
    const std::unordered_map<Name, FnSig> &sigs;

public:
    /// Lifetime must be within that of fn and sigs.
    Typer(const Function &fn, const std::unordered_map<Name, FnSig> &sigs)
    : fn(fn), sigs(sigs) {
        for (const Param &param : fn.params) {
            assignType(param.id.name, param.type);
        }
    }

    std::optional<Err> type(const Statement &s) { return std::visit(*this, s); }

    const SymbolTable::TypeMap &types(void) const { return _types; }

    // Delete catch-all so compiler catches missing overloads.
    template<class T> std::optional<Err> operator()(const T &) = delete;

    // These operator() are public for std::visit.

    std::optional<Err> operator()(const PhiStmt &phi) {
        const std::vector<Val> &vs = usedVals(phi);
        std::vector<Type> vts;
        for (const Val &v : vs) { vts.push_back(valType(v)); }
        if (
            !std::all_of(
                vts.cbegin(),
                vts.cend(),
                // vts[0] okay, t only exists when vts is non-empty.
                [&vts](const Type &t) { return t == vts[0]; }
            )
        ) {
            return Err(
                "The operand types of a phi statement must all be the same. "
                "Operand types, respectively, are " + typesToString(vts) +
                " in '" + pretty(phi) + "'.",
                span(phi)
            );
        }

        assignType(phi.id.name, vts.front());

        return std::nullopt;
    }

    std::optional<Err> operator()(const BinaryOpStmt &binop) {
        const Type &lt = valType(binop.left);
        const Type &rt = valType(binop.right);

        if (lt != rt) {
            return Err(
                "Expected operands to be of the same type, not "
                "'" + pretty(lt) + "' and '" + pretty(rt) + "' "
                "in '" + pretty(binop) + "'.",
                span(binop)
            );
        }

        static const auto expErr = [](
            const std::string &exp,
            const Type &actual,
            const BinaryOpStmt &binop
        ) {
            return Err(
                "Expected " + exp + " operands , not '" + pretty(actual) +
                "' in '" + pretty(binop) + "'.",
                span(binop)
            );
        };

        switch (binop.op) {
        case BinaryOpStmt::Op::ADD:
        case BinaryOpStmt::Op::SUB:
        case BinaryOpStmt::Op::MUL:
            if (!is<IntType>(lt) && !is<FloatType>(lt)) {
                return expErr("integer or float", lt, binop);
            }
            assignType(binop.id.name, lt);
            break;

        case BinaryOpStmt::Op::DIV:
        case BinaryOpStmt::Op::REM:
            if (!is<FloatType>(lt)) { return expErr("float", lt, binop); }
            assignType(binop.id.name, lt);
            break;

        case BinaryOpStmt::Op::UDIV:
        case BinaryOpStmt::Op::SDIV:
        case BinaryOpStmt::Op::UREM:
        case BinaryOpStmt::Op::SREM:
            if (!is<IntType>(lt)) { return expErr("integer", lt, binop); }
            assignType(binop.id.name, lt);
            break;

        case BinaryOpStmt::Op::EQ:
        case BinaryOpStmt::Op::NEQ:
            if (!is<IntType>(lt) && !is<FloatType>(lt) && !is<PtrType>(lt)) {
                return expErr("integer, float, or pointer", lt, binop);
            }
            assignType(binop.id.name, FAB_INT8_TYPE);
            break;

        case BinaryOpStmt::Op::LT:
        case BinaryOpStmt::Op::LE:
        case BinaryOpStmt::Op::GT:
        case BinaryOpStmt::Op::GE:
            if (!is<FloatType>(lt)) { return expErr("float", lt, binop); }
            assignType(binop.id.name, FAB_INT8_TYPE);
            break;

        case BinaryOpStmt::Op::SLT:
        case BinaryOpStmt::Op::SLE:
        case BinaryOpStmt::Op::SGT:
        case BinaryOpStmt::Op::SGE:
        case BinaryOpStmt::Op::ULT:
        case BinaryOpStmt::Op::ULE:
        case BinaryOpStmt::Op::UGT:
        case BinaryOpStmt::Op::UGE:
            if (!is<IntType>(lt)) { return expErr("integer", lt, binop); }
            assignType(binop.id.name, FAB_INT8_TYPE);
            break;

        case BinaryOpStmt::Op::AND:
        case BinaryOpStmt::Op::OR:
        case BinaryOpStmt::Op::XOR:
        case BinaryOpStmt::Op::SHIFTL:
        case BinaryOpStmt::Op::ASHIFTR:
        case BinaryOpStmt::Op::LSHIFTR:
            if (!is<IntType>(lt)) { return expErr("integer", lt, binop); }
            assignType(binop.id.name, lt);
            break;

        default: unreachable();
        }

        return std::nullopt;
    }

    std::optional<Err> operator()(const UnaryOpStmt &unop) {
        const Type &vt = valType(unop.val);
        switch (unop.op) {
        case UnaryOpStmt::Op::NOT:
            if (!is<IntType>(vt)) {
                return Err(
                    "Expected operand to be of an integer type, not a '" +
                    pretty(vt) + "' in '" + pretty(unop) + "'.",
                    span(unop.val)
                );
            }

            assignType(unop.id.name, vt);
            break;
        default: unreachable();
        }

        return std::nullopt;
    }

    std::optional<Err> operator()(const CallStmt &call) {
        std::vector<Type> ats;
        for (const Val &a : call.args) { ats.push_back(valType(a)); }
        const Type &ret =
            call.tid.has_value() ? call.tid.value().type : FAB_VOID_TYPE;
        if (
            !std::any_of(
                sigs.cbegin(),
                sigs.cend(),
                [&ats, &ret](const auto &ns) {
                    return ns.second.matches(ats, ret);
                }
            )
        ) {
            return Err(
                "The combination of return type '" + pretty(ret) + "' and "
                "argument types " + typesToString(ats) + " do not match any "
                "function signature in the program, as in '" + pretty(call) +
                "'.",
                span(call)
            );
        }

        // TODO: if we have a function name as the callee, should we check
        //       against it? A warning at least could be nice but we have
        //       no warning facilities right now.

        if (call.tid.has_value()) {
            assignType(call.tid.value().id.name, call.tid.value().type);
        }

        return std::nullopt;
    }

    std::optional<Err> operator()(const RetStmt &ret) {
        if (ret.val.has_value()) {
            const Type &rt = valType(ret.val.value());
            if (rt != fn.type) {
                return Err(
                    "Attempt to return a '" + pretty(rt) + " from function '" +
                    fn.id.name + "' which returns a '" + pretty(fn.type) + "' "
                    "at '" + pretty(ret) + "'.",
                    span(ret)
                );
            }
        }
        return std::nullopt;
    }

    std::optional<Err> operator()(const BrStmt &br) { return std::nullopt; }

    std::optional<Err> operator()(const BrifStmt &brif) {
        const Type &ct = valType(brif.cond);
        if (!is<IntType>(ct)) {
            return Err(
                "Expected condition to be of an integer type, not a '" +
                pretty(ct) + "' in '" + pretty(brif) + "'.",
                span(brif.cond)
            );
        }

        return std::nullopt;
    }

    std::optional<Err> operator()(const AllocStmt &alloc) {
        assignType(introducedVar(alloc).value().name, FAB_PTR_TYPE);
        return std::nullopt;
    }

    std::optional<Err> operator()(const StoreStmt &store) {
        const std::optional<Err> &pe =
            checkPVal(store.dst, store, "destination pointer");
        if (pe.has_value()) { return pe.value(); }
        return std::nullopt;
    }

    std::optional<Err> operator()(const LoadStmt &load) {
        const std::optional<Err> &pe =
            checkPVal(load.src, load, "source pointer");
        if (pe.has_value()) { return pe.value(); }

        assignType(load.tid.id.name, load.tid.type);
        return std::nullopt;
    }

    std::optional<Err> operator()(const FieldStmt &field) {
        const std::optional<Err> &pe =
            checkPVal(field.src, field, "source pointer");
        if (pe.has_value()) { return pe.value(); }

        const std::optional<Err> &ie = checkIVal(field.index, field, "index");
        if (ie.has_value()) { return ie.value(); }

        assignType(field.id.name, FAB_PTR_TYPE);
        return std::nullopt;
    }

    std::optional<Err> operator()(const BitCastStmt &bitcast) {
        const Type &fromt = valType(bitcast.val);
        if (!is<IntType>(fromt) && !is<FloatType>(fromt)) {
            return Err(
                "Only integer and float values may be bitcast, not '" +
                pretty(fromt) + "' values as in '" + pretty(bitcast) + "'.",
                span(bitcast.val)
            );
        }

        const Type &tot = bitcast.type;
        if (!is<IntType>(bitcast.type) && !is<FloatType>(bitcast.type)) {
            return Err(
                "Values may only be bitcast to integer or float types, not "
                "a '" + pretty(tot) + "' type as in '" + pretty(bitcast) + "'.",
                span(tot)
            );
        }

        unsigned int fromw;
        if (is<IntType>(fromt)) { fromw = get<IntType>(fromt).width(); }
        else if (is<FloatType>(fromt)) {
            fromw = get<FloatType>(fromt).width();
        } else { unreachable(); }

        unsigned int tow;
        if (is<IntType>(tot)) { tow = get<IntType>(tot).width(); }
        else if (is<FloatType>(tot)) {
            tow = get<FloatType>(tot).width();
        } else { unreachable(); }

        if (fromw != tow) {
            return Err(
                "The type bitcast from and to must have the same width, unlike "
                "'" + pretty(fromt) + "' and '" + pretty(tot) + " as in '" +
                pretty(bitcast) + "'.",
                span(bitcast)
            );
        }

        assignType(bitcast.id.name, bitcast.type);

        return std::nullopt;
    }

    std::optional<Err> operator()(const ConvertStmt &convert) {
        const Type &fromt = valType(convert.val);
        const Type &tot = convert.type;
        if (
            (is<FloatType>(fromt) && is<FloatType>(tot)) ||
            (is<IntType>(fromt) && is<PtrType>(tot)) ||
            (is<PtrType>(fromt) && is<IntType>(tot)) ||
            (is<IntType>(fromt) && is<FloatType>(tot)) ||
            (is<FloatType>(fromt) && is<IntType>(tot)) ||
            (fromt == tot)
        ) {
            if (convert.extKind.has_value()) {
                return Err(
                    "An extension kind is only to be provided when converting "
                    "converting from a smaller integer type to a larger "
                    "integer type, unlike in '" + pretty(convert) + "'.",
                    span(tot)
                );
            }

            // Otherwise, these conversions are always okay.
        } else if (is<IntType>(fromt) && is<IntType>(tot)) {
            const IntType &fromit = get<IntType>(fromt);
            const IntType &toit = get<IntType>(tot);

            if (toit.width() > fromit.width()) {
                if (!convert.extKind.has_value()) {
                    return Err(
                        "An extension kind ('sign' or 'zero') must be provided "
                        "when converting from a smaller integer type ('" +
                        pretty(fromit) + "') to a larger integer type ('" +
                        pretty(toit) + "'), unlike in '" + pretty(convert) +
                        "'.",
                        span(toit)
                    );
                }
            }
        } else {
            return Err(
                "Conversion from '" + pretty(fromt) + "' to '" + pretty(tot) +
                "' is invalid, as in '" + pretty(convert) + "'.",
                span(convert)
            );
        }

        assignType(convert.id.name, convert.type);
        return std::nullopt;
    }

    std::optional<Err> operator()(const VarargStmt &vararg) {
        const std::optional<Err> &ie = checkIVal(vararg.index, vararg, "index");
        if (ie.has_value()) { return ie.value(); }

        assignType(vararg.tid.id.name, vararg.tid.type);
        return std::nullopt;
    }

    std::optional<Err> operator()(const BlackholeStmt &bh) {
        if (bh.tid.has_value()) {
            const TypedId &tid = bh.tid.value();
            assignType(tid.id.name, tid.type);
        }
        return std::nullopt;
    }

private:
    /// The type of val. Assumes that if val is an ID, the defining statement
    /// for ID has been previously checked (operator()).
    Type valType(const Val &val) const {
        if (is<Id>(val)) {
            const Id &id = get<Id>(val);
            if (id.isGlobal()) { return FAB_PTR_TYPE; }
            else {
                insist(id.isLocal());
                insist(_types.count(id.name));
                return _types.at(id.name);
            }
        } else {
            insist(is<TypedConstant>(val));
            return get<TypedConstant>(val).type;
        }
    }

    /// Set _types[n] = t, ensuring t is not already in _types.
    void assignType(const Name &n, const Type &t) {
        insist(!_types.count(n));
        _types.insert({n, t});
    }

    /// Return an error when pv does not have a pointer type. s is the
    /// containing statement and what is a description of what pv represents.
    /// Both are used for error messages.
    std::optional<Err> checkPVal(
        const PVal &pv,
        const Statement &s,
        const std::string &what
    ) {
        if (is<Id>(pv)) {
            const Type &t = valType(get<Id>(pv));
            if (!is<PtrType>(t)) {
                return Err(
                    "Expected " + what + " to be of pointer type, not a '" +
                    pretty(t) + "' in '" + pretty(s) + "'.",
                    span(pv)
                );
            }
        }

        return std::nullopt;
    }

    /// Return an error when iv does not have an integer type. s is the
    /// containing statement and what is a description of what iv represents.
    /// Both are used for error messages.
    std::optional<Err> checkIVal(
        const IVal &iv,
        const Statement &s,
        const std::string &what
    ) {
        if (is<Id>(iv)) {
            const Type &t = valType(get<Id>(iv));
            if (!is<PtrType>(t)) {
                return Err(
                    "Expected " + what + " to be of pointer type, not a '" +
                    pretty(t) + "' in '" + pretty(s) + "'.",
                    span(iv)
                );
            }
        }

        return std::nullopt;
    }

    /// Of the form, "'t1', 't2', and 't3'", with proper grammar.
    static std::string typesToString(const std::vector<Type> &ts) {
        std::string s;
        for (size_t i = 0; i < ts.size(); ++i) {
            s += "'" + pretty(ts[i]) + "'";
            if (ts.size() == 2 && i == 0) {
                s += " and ";
            } else if (i == ts.size() - 2) {
                s += ", and ";
            } else if (i != ts.size() - 1) {
                s += ", ";
            }
        }

        return s;
    }
};

/// Return an ordering of the blocks in fn such that for all i, there are no two
/// blocks order[i] and order[i+n] (positive n) such that a variable is defined
/// in the latter and used in the former.
std::vector<Index> _defUseBlockOrder(const Function &fn) {
    // The order will be such that the higher nodes in a dominator tree come
    // first. Unreachable will come last (they can be anywhere). This works
    // because we checked that def-before-use is correct thus any def
    // dominates any use.
    std::vector<Index> order;
    if (fn.isOpaque()) { return order; }

    using Node = DominatorTree::Node;

    const ControlFlowGraph &cfg = ControlFlowGraph(fn.body.value());
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    const std::function<void(const Node &)> visit =
    [&order, &visit](const Node &n) {
        order.push_back(n.id);
        for (const Node &c : n.children) { visit(c); }
    };

    visit(dt.root);
    for (const Node &u : dt.unreachables) { order.push_back(u.id); }

    return order;
}

}  // Anonymous namespace.

namespace SVFIR {

Result<SymbolTable, Err> buildSymbolTable(const Program &p) {
    SymbolTable::SigMap sigs;
    for (const Function &fn : p.functions) {
        insist(sigs.count(fn.id.name) == 0);
        sigs.insert({fn.id.name, FnSig(fn)});
    }

    SymbolTable::FnTables fnTables;
    for (const Function &fn : p.functions) {
        // If we follow the def-use order, we can type the entire program in a
        // single linear pass. Variables are defined once, so we never clash, no
        // need to worry about scope.
        const std::vector<Index> &order = _defUseBlockOrder(fn);
        Typer typer(fn, sigs);
        for (const Index bbi : order) {
            const BasicBlock &bb = fn.body.value()[bbi];
            for (const Statement &s : bb.stmts) {
                const std::optional<Err> &te = typer.type(s);
                if (te.has_value()) { return te.value(); }
            }
        }

        insist(fnTables.count(fn.id.name) == 0);
        fnTables.insert({fn.id.name, typer.types()});
    }

    return SymbolTable(sigs, fnTables);
}

Result<TypedProgram, Err> typeProgram(const Program &p) {
    const std::optional<Err> &ae = analyse(
        p,
        {
            checkConstantTypes,
            checkTerminatingStmts,
            checkDuplicateDecs,
            checkVarargStmts,
            checkUndeclaredBlocks,
            checkUndeclaredGlobals,
            checkEntryPredecessors,
            checkPhiStmtArgs,
            checkLocalDefBeforeUse
        }
    );
    if (ae.has_value()) { return ae.value(); }

    const Result<SymbolTable, Err> &syms = buildSymbolTable(p);
    if (is<Err>(syms)) { return get<Err>(syms); }
    else { return TypedProgram(p, getVal(syms)); }
    unreachable();
}


}  // namespace SVFIR.
