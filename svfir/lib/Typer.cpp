#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "Contracts.h"
#include "ControlFlowGraph.h"
#include "Dominator.h"
#include "Ops.h"
#include "Err.h"
#include "Index.h"
#include "Result.h"
#include "Statements.h"
#include "TypedProgram.h"
#include "Variant.h"

namespace {

using namespace SVFIR;

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkDuplicateDecs
/// DEPS: --
/// Raise an error if duplicate IDs are declared. This comes down to:
///  * Duplicately named functions and global variables (same namespace).
///  * Duplicately named local variables within a function, including paramters.
///  * Duplicately named blocks within a function.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> _checkDuplicateGlobalDecs(
    const std::vector<GlobalVar> &vs,
    const std::vector<Function> &fns
) {
    std::unordered_set<Name> names;
    for (const GlobalVar &v : vs) {
        const Name name = v.id.name;
        if (!names.insert(name).second) {
            return Err(
                "Global variable name '" + name + "' declared twice", v.span
            );
        }
    }
    for (const Function &fn : fns) {
        const Name name = fn.id.name;
        if (!names.insert(name).second) {
            return Err("Function name '" + name + "' declared twice", fn.span);
        }
    }
    return std::nullopt;
}

std::optional<Err> _checkDuplicateLocalDecs(const Function &fn) {
    std::unordered_set<Name> names;
    for (const auto &p : fn.params) {
        const Name name = p.id.name;
        if (!names.insert(name).second) {
            return Err("Parameter '" + name + "' declared twice", fn.span);
        }
    }

    if (fn.body.has_value()) {
        for (const auto &bb : fn.body.value()) {
            for (const auto &s : bb.stmts) {
                const std::optional<Id> id = introducedVar(s);
                if (id.has_value()) {
                    const Name name = id.value().name;
                    if (!names.insert(name).second) {
                        return Err(
                            "Variable '" + name + "' declared twice", span(s)
                        );
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> _checkDuplicateBlockDecs(const Function &fn) {
    std::unordered_set<Name> names;
    if (fn.body.has_value()) {
        for (const auto &bb : fn.body.value()) {
            const Name &name = bb.id.name;
            if (names.find(name) != names.cend()) {
                return Err("Block '" + name + "' declared twice", span(bb));
            } else { names.insert(name); }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkDuplicateDecs(const Program &p) {
    const std::optional<Err> gs = _checkDuplicateGlobalDecs(
        p.globalVars, p.functions
    );
    if (gs.has_value()) { return gs.value(); }

    for (const Function &fn : p.functions) {
        const std::optional<Err> ls = _checkDuplicateLocalDecs(fn);
        if (ls.has_value()) { return ls.value(); }

        const std::optional<Err> bs = _checkDuplicateBlockDecs(fn);
        if (bs.has_value()) { return bs.value(); }
    }

    return std::nullopt;
}


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

std::optional<Err> checkTerminatingStmts(const Program &p) {
    for (const auto &fn : p.functions) {
        if (fn.body.has_value()) {
            for (const auto &bb : fn.body.value()) {
                for (auto it = bb.stmts.cbegin(); it != bb.stmts.cend(); ++it) {
                    if (it + 1 != bb.stmts.cend()) {
                        // Not at the end? Must not terminate.
                        if (isTerminatingStmt(*it)) {
                            return Err(
                                "Terminating statement '" +
                                pretty(*it) +
                                "' not final statement of basic block",
                                span(*it)
                            );
                        }
                    } else {
                        // At the end? Must terminate.
                        if (!isTerminatingStmt(*it)) {
                            return Err(
                                "Non-terminating statement '" +
                                pretty(*it) +
                                "' as final statement of basic block",
                                span(*it)
                            );
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkEntryPredecessors
/// DEPS: checkTerminatingStmts
/// Raise an error if the entry block has a predecessor.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> checkEntryPredecessors(const Program &p) {
    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            const Name &entry = fn.body.value().front().id.name;
            for (const BasicBlock &bb : fn.body.value()) {
                const Statement &ts = bb.stmts.back();
                insist(isTerminatingStmt(ts));
                std::vector<Name> targets;
                if (is<BrStmt>(ts)) {
                    targets.push_back(get<BrStmt>(ts).target.name);
                } else if (is<BrifStmt>(ts)) {
                    const BrifStmt &brif = get<BrifStmt>(ts);
                    targets.push_back(brif.ifTarget.name);
                    targets.push_back(brif.elseTarget.name);
                } else { insist(is<RetStmt>(ts)); }

                for (const Name &target : targets) {
                    if (target == entry) {
                        return Err(
                            "The entry block '" + entry + "' of function '" +
                            fn.id.name + "' is targeted by '" + pretty(ts) +
                            "'. Entry blocks cannot have predecessors.",
                            span(ts)
                        );
                    }
                }
            }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkVarargStmts
/// DEPS: --
/// Raise an error if a vararg statement is used in a non-vararg function.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> checkVarargStmts(const Program &p) {
    for (const auto &fn : p.functions) {
        if (fn.vararg && fn.body.has_value()) {
            for (const auto &bb : fn.body.value()) {
                for (const auto &s : bb.stmts) {
                    if (is<VarargStmt>(s)) {
                        return Err(
                            "vararg statement '" +
                            pretty(s) +
                            "' in non-vararg function '" +
                            pretty(fn.id) +
                            "'.",
                            span(s)
                        );
                    }
                }
            }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkUndeclaredBlocks
/// DEPS: --
/// Raise an error if branches are used incorrectly. This comes down to:
///  * br and brif instructions targeting non-existent branches.
///  * phi instructions referring to non-existent blocks in args.
////////////////////////////////////////////////////////////////////////////////

Err _nonExistentBlock(const Name &name, const Span &span) {
    return Err("Block '" + name + "' does not exist.", span);
}

std::optional<Err> _checkUndeclaredBlocks(const FunctionBody &bbs) {
    std::unordered_set<Name> blocks;
    for (const BasicBlock &bb : bbs) { blocks.insert(bb.id.name); }

    for (size_t i = 0; i < bbs.size(); ++i) {
        const BasicBlock &bb = bbs[i];
        for (const Statement &s : bb.stmts) {
            for (const Id &blk : usedBlocks(s)) {
                if (blocks.find(blk.name) == blocks.cend()) {
                    return _nonExistentBlock(blk.name, blk.span);
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkUndeclaredBlocks(const Program &p) {
    for (const auto &fn : p.functions) {
        if (fn.body.has_value()) {
            const std::optional<Err> be =
                _checkUndeclaredBlocks(fn.body.value());
            if (be.has_value()) { return be.value(); }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkPhiStmtArgs
/// DEPS: checkUndeclaredBlocks
/// Raise an error if a phi statement is ill-formed. This comes down to having
/// an argument for each predecessor block of the block the phi statement is in.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> _checkPhiStmtArgs(const FunctionBody &bbs) {
    const ControlFlowGraph cfg = ControlFlowGraph(bbs);
    for (Index i = 0; i < bbs.size(); ++i) {
        const BasicBlock &bb = bbs[i];
        for (const Statement &s : bb.stmts) {
            if (is<PhiStmt>(s)) {
                const PhiStmt &phi = get<PhiStmt>(s);

                // We will go through the actual args, erasing the defining
                // block from the set of real predecessors. We have an error if
                //  1) we try to erase something not in the set (we'll keep
                //     track of what has been deleted to detect duplicate
                //     args too), or
                //  2) after all is said and done, we haven't erased everything.
                // We could just compare sorted vectors of real vs. phi
                // predecessors but good errors would be hard.
                std::unordered_set<Name> preds;
                for (const Index pi : cfg.node(i).inEdges) {
                    preds.insert(bbs[pi].id.name);
                }

                std::unordered_set<Name> seen;
                for (const PhiStmt::Arg &arg : phi.args) {
                    const Name &argName = arg.pred.name;
                    if (seen.find(argName) != seen.cend()) {
                        return Err(
                            "Duplicate argument for predecessor block '" +
                            argName + "' in phi statement",
                            arg.span
                        );
                    } else { seen.insert(argName); }
                    if (!preds.erase(argName)) {
                        return Err(
                            "Phi argument block '" + argName + "' is not a "
                            "predecessor block",
                            arg.span
                        );
                    }
                }
                if (!preds.empty()) {
                    std::string uncovered;
                    for (const Name &pred : preds) {
                        if (!uncovered.empty()) { uncovered += ", "; }
                        uncovered += "'" + pred + "'";
                    }
                    return Err(
                        "Phi statement arguments are not comprehensive. There "
                        "is no corresponding argument for predecessor block" +
                        (preds.size() == 1 ? " " + uncovered : "s " + uncovered),
                        phi.span
                    );
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkPhiStmtArgs(const Program &p) {
    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            const std::optional<Err> pe = _checkPhiStmtArgs(fn.body.value());
            if (pe.has_value()) { return pe.value(); }
        }
    }

    return std::nullopt;
}

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

/// Where a statement sits so we can check for use-before-def.
struct StmtLoc final {
    /// Position within function.
    const Index block;
    /// Position within block.
    const Index stmt;
    StmtLoc(const Index block, const Index stmt) : block(block), stmt(stmt) { }

    /// Does sl occur, strictly, after this location?
    bool after(const DominatorTree &dt, const StmtLoc &sl) const {
        if (this->block == sl.block) { return this->stmt > sl.stmt; }
        else { return dt.dominates(sl.block, this->block); }
        unreachable();
    }
};

/// Returns a map of local variables to (the basic block they were defined in,
/// the statement in that basic block they were defined in).
std::unordered_map<Name, StmtLoc> _localDefSites(const FunctionBody &bbs) {
    std::unordered_map<Name, StmtLoc> defs;
    for (Index bbi = 0; bbi < bbs.size(); ++bbi) {
        const BasicBlock &bb = bbs[bbi];
        for (Index si = 0; si < bb.stmts.size(); ++si) {
            const Statement &s = bb.stmts[si];
            const std::optional<Id> var = introducedVar(s);
            if (var.has_value()) {
                defs.insert({var.value().name, StmtLoc(bbi, si)});
            }
        }
    }
    return defs;
}

std::optional<Err> _checkLocalDefBeforeUse(
    const FunctionBody &bbs,
    std::unordered_set<Name> params
) {
    const std::unordered_map<Name, StmtLoc> defs = _localDefSites(bbs);
    const ControlFlowGraph &cfg = ControlFlowGraph(bbs);
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    for (Index bbi = 0; bbi < bbs.size(); ++bbi) {
        const BasicBlock &bb = bbs[bbi];
        for (Index si = 0; si < bb.stmts.size(); ++si) {
            const Statement &s = bb.stmts[si];
            const StmtLoc sl = StmtLoc(bbi, si);
            const std::vector<Id> uses = directlyUsedVars(s);

            for (const Id &u : uses) {
                if (
                    u.isLocal() &&
                    params.find(u.name) == params.cend() &&
                    defs.find(u.name) == defs.cend()
                ) {
                    return Err(
                        "Local variable '" + u.name + "' not defined.",
                        span(u)
                    );
                }
            }

            if (!is<PhiStmt>(s)) {
                for (const Id &u : directlyUsedVars(s)) {
                    if (
                        u.isLocal() &&
                        params.find(u.name) == params.cend() &&
                        !sl.after(dt, defs.at(u.name))
                    ) {
                        return Err(
                            "Local variable '" + u.name + "' used "
                            "before definition in '" + pretty(s) + "'.",
                            span(u)
                        );
                    }
                }
            } else {
                insist(is<PhiStmt>(s));
                const PhiStmt &phi = get<PhiStmt>(s);

                // There is one argument for each predecessor
                // (checkPhiStmtArgs). We need their indexes to check dominance.
                std::unordered_map<Name, Index> preds;
                for (const Index pred : cfg.node(bbi).inEdges) {
                    preds[cfg.node(pred).name] = pred;
                }

                for (const PhiStmt::Arg &arg : phi.args) {
                    if (is<Id>(arg.val) && get<Id>(arg.val).isLocal()) {
                        const Id &var = get<Id>(arg.val);
                        const Index pi = preds.at(arg.pred.name);
                        const StmtLoc &dl = defs.at(var.name);
                        // The use is in this block, not pred, so we check that
                        // we are either defined in pred or some dominator. If
                        // it's a parameter of course, no problem.
                        if (
                            params.find(var.name) == params.cend() &&
                            pi != dl.block &&
                            !dt.dominates(dl.block ,pi)
                        ) {
                            return Err(
                                "Local variable '" + var.name + "' in phi "
                                "argument was not defined in or strictly "
                                "before predecessor '" + arg.pred.name + "'.",
                                arg.span
                            );
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkLocalDefBeforeUse(const Program &p) {
    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            std::unordered_set<Name> params;
            for (const Param &param : fn.params) {
                params.insert(param.id.name);
            }

            const std::optional<Err> e =
                _checkLocalDefBeforeUse(fn.body.value(), params);
            if (e.has_value()) { return e.value(); }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkGlobalDefBeforeUse
/// DEPS: --
/// Raises an error if any global variable is used without being defined.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> checkGlobalDefBeforeUse(const Program &p) {
    std::unordered_set<Name> globals;
    for (const GlobalVar &gv : p.globalVars) { globals.insert(gv.id.name); }
    for (const Function &fn : p.functions) { globals.insert(fn.id.name); }

    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            for (const BasicBlock &bb : fn.body.value()) {
                for (const Statement &s : bb.stmts) {
                    for (const Id &u : directlyUsedVars(s)) {
                        if (
                            u.isGlobal() &&
                            globals.find(u.name) == globals.cend()
                        ) {
                            return Err(
                                "Global variable '" + u.name + "' used but "
                                "not defined.",
                                span(u)
                            );
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// PASS: buildSymbolTable
/// DEPS: ALL OTHER PASSES
/// Type checks the program, raising an error on any type error. Otherwise
/// returns a symbol table for the program. Type checking involves:
///  * That operands of statements (including types in convert/bitcast)
///    are appropriately typed.
///    * For calls, this involves checking that *some* function exists with
///      a matching signature.
///  * That typed constants carry appropriate types.
////////////////////////////////////////////////////////////////////////////////

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

/// To be used by the typeProgram pass. type() should be cotinually called on
/// statements in def-use order. At the end of this, a mapping of local names
/// to types can then be pulled out via types().
struct Typer {
private:
    SymbolTable::TypeMap _types;
    /// Function we're typing.
    const Function &fn;
    // TODO: this keeps getting copied, we need to measure performance.
    const std::unordered_map<Name, FnSig> sigs;

public:
    Typer(const Function &fn, const std::unordered_map<Name, FnSig> &sigs)
    : fn(fn), sigs(sigs) {
        for (const Param &param : fn.params) {
            assignType(param.id.name, param.type);
        }
    }

    std::optional<Err> type(const Statement &s) {
        const std::optional<Err> &cte = checkConstantTypes(s);
        if (cte.has_value()) { return cte.value(); }

        return std::visit(*this, s);
    }

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

        // TODO: it is easier that this check is first (only ever deal with
        //       one type elsewhere) but unideal from the perspective of error
        //       messages. Consider an add instruction with an agg and a ptr
        //       operand, we'd get an inequality error first but really they're
        //       just bad operands, equal or not.
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

        case BinaryOpStmt::Op::UDIV:
        case BinaryOpStmt::Op::SDIV:
        case BinaryOpStmt::Op::UREM:
        case BinaryOpStmt::Op::SREM:
            if (!is<IntType>(lt) && !is<FloatType>(lt)) {
                return expErr("integer or float", lt, binop);
            }
            assignType(binop.id.name, lt); // TODO WRONG
            break;

        case BinaryOpStmt::Op::EQ:
        case BinaryOpStmt::Op::NEQ:
            if (!is<IntType>(lt) && !is<FloatType>(lt) && !is<PtrType>(lt)) {
                return expErr("integer, float, or pointer", lt, binop);
            }
            assignType(binop.id.name, FabInt8Type);
            break;

        case BinaryOpStmt::Op::LT:
        case BinaryOpStmt::Op::LE:
        case BinaryOpStmt::Op::GT:
        case BinaryOpStmt::Op::GE:
            if (!is<FloatType>(lt)) { return expErr("float", lt, binop); }
            assignType(binop.id.name, FabInt8Type);
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
            assignType(binop.id.name, FabInt8Type);
            break;

        case BinaryOpStmt::Op::AND:
        case BinaryOpStmt::Op::OR:
        case BinaryOpStmt::Op::XOR:
        case BinaryOpStmt::Op::SHIFTL:
        case BinaryOpStmt::Op::ASHIFTR:
        case BinaryOpStmt::Op::LSHIFTR:
            if (!is<IntType>(lt)) { return expErr("integer", lt, binop); }
            assignType(binop.id.name, FabInt8Type);
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
            call.tid.has_value() ? call.tid.value().type : FabVoidType;
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
        assignType(introducedVar(alloc).value().name, FabPtrType);
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

        assignType(field.id.name, FabPtrType);
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
            (is<FloatType>(fromt) && is<IntType>(tot))
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
    const PtrType FabPtrType = PtrType(FAB_SPAN);
    const IntType FabInt8Type = IntType(IntType::Kind::I8, FAB_SPAN);
    const VoidType FabVoidType = VoidType(FAB_SPAN);

private:
    std::optional<Err> checkConstantTypes(const Statement &s) const {
        // TODO
        return std::nullopt;
    }

    /// The type of val. Assumes
    ///  1) if val is a constant type it has been previously checked
    ///     (checkConstantTypes), and
    ///  2) if val is an ID, the defining statement for ID has been
    ///     previously checked (operator()).
    ///  ^ TODO: put (1) more?
    Type valType(const Val &val) const {
        if (is<Id>(val)) {
            const Id &id = get<Id>(val);
            if (id.isGlobal()) { return FabPtrType; }
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

Result<SymbolTable, Err> buildSymbolTable(const Program &p) {
    SymbolTable::SigMap sigs;
    for (const Function &fn : p.functions) {
        insist(sigs.count(fn.id.name) == 0);
        sigs.insert({fn.id.name, FnSig(fn)});
    }

    SymbolTable::FnTables fnTables;
    for (Index fi = 0; fi < p.functions.size(); ++fi) {
        const Function &fn = p.functions[fi];
        // If we follow the def-use order, we can type the entire program in a
        // single linear pass. Variables are defined once, so we never clash, no
        // need to worry about scope.
        const std::vector<Index> &order = _defUseBlockOrder(fn);
        Typer typer(fn, sigs);
        for (const Index bbi : order) {
            const BasicBlock &bb = fn.body.value()[bbi];
            for (const Statement &s : bb.stmts) {
                const std::optional<Err> te = typer.type(s);
                if (te.has_value()) { return te.value(); }
            }
        }

        insist(fnTables.count(fn.id.name) == 0);
        fnTables.insert({fn.id.name, typer.types()});
    }

    return SymbolTable(sigs, fnTables);
}

}  // Anonymous namespace

namespace SVFIR {

FnSig::FnSig(const Function &fn)
: params(fn.paramTypes()), ret(fn.type), vararg(fn.vararg)
{ }

bool FnSig::matches(const std::vector<Type> &args, const Type &exp) const {
    if (exp != ret) { return false; }
    else if (args.size() < params.size()) { return false; }
    else if (!vararg && args.size() != params.size()) { return false; }
    else {
        // The remaining cases are
        //   1. args and params have the same size (regardless of varargs),
        //   2. args > params and varargs
        // In both instances, we only need to concern ourselves with the
        // non-varargs part.
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i] != args[i]) { return false; }
        }

        return true;
    }
    unreachable();
}

bool isTerminatingStmt(const Statement &stmt) {
    return (
        is<BrStmt>(stmt) ||
        is<BrifStmt>(stmt) ||
        is<RetStmt>(stmt)
    );
}

Result<TypedProgram, Err> typeProgram(const Program &p) {
    std::optional<Err> de = checkDuplicateDecs(p);
    if (de.has_value()) { return de.value(); }

    std::optional<Err> te = checkTerminatingStmts(p);
    if (te.has_value()) { return te.value(); }

    std::optional<Err> epe = checkEntryPredecessors(p);
    if (epe.has_value()) { return epe.value(); }

    std::optional<Err> ve = checkVarargStmts(p);
    if (ve.has_value()) { return ve.value(); }

    std::optional<Err> be = checkUndeclaredBlocks(p);
    if (be.has_value()) { return be.value(); }

    std::optional<Err> pe = checkPhiStmtArgs(p);
    if (pe.has_value()) { return pe.value(); }

    std::optional<Err> gdbue = checkGlobalDefBeforeUse(p);
    if (gdbue.has_value()) { return gdbue.value(); }

    std::optional<Err> ldbue = checkLocalDefBeforeUse(p);
    if (ldbue.has_value()) { return ldbue.value(); }

    const Result<SymbolTable, Err> &syms = buildSymbolTable(p);
    if (is<Err>(syms)) { return get<Err>(syms); }
    else { return TypedProgram(p, getVal(syms)); }

    unreachable();
}

}  // namespace SVFIR
