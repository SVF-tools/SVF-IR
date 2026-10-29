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
///  * Duplicately named functions and variables (same namespace).
///  * Duplicately named local variables within a function, including paramters.
///  * Duplicately named blocks within a function.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> _checkDuplicateGlobalDecs(
    const std::vector<Variable> &vs,
    const std::vector<Function> &fns
) {
    std::unordered_set<Name> names;
    for (const auto &v : vs) {
        const Name name = v.id.name;
        if (names.insert(name).second) {
            return Err("Variable name '" + name + "' declared twice", v.span);
        }
    }
    for (const auto &fn : fns) {
        const Name name = fn.id.name;
        if (names.insert(name).second) {
            return Err("Function name '" + name + "' declared twice", fn.span);
        }
    }
    return std::nullopt;
}

std::optional<Err> _checkDuplicateLocalDecs(const Function &fn) {
    std::unordered_set<Name> names;
    for (const auto &p : fn.params) {
        const Name name = p.id.name;
        if (names.insert(name).second) {
            return Err("Parameter '" + name + "' declared twice", fn.span);
        }
    }

    if (fn.body.has_value()) {
        for (const auto &bb : fn.body.value()) {
            for (const auto &s : bb.stmts) {
                const std::optional<Id> id = introducedVar(s);
                if (id.has_value()) {
                    const Name name = id.value().name;
                    if (names.insert(name).second) {
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
        p.variables,
        p.functions
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
        if (this->block == sl.block) { return sl.stmt > this->stmt; }
        else { return dt.dominates(this->block, sl.block); }
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

std::optional<Err> _checkLocalDefBeforeUse(const FunctionBody &bbs) {
    const std::unordered_map<Name, StmtLoc> defs = _localDefSites(bbs);
    const ControlFlowGraph &cfg = ControlFlowGraph(bbs);
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    for (Index bbi = 0; bbi < bbs.size(); ++bbi) {
        const BasicBlock &bb = bbs[bbi];
        for (Index si = 0; si < bb.stmts.size(); ++si) {
            const Statement &s = bb.stmts[si];
            const StmtLoc sl = StmtLoc(bbi, si);
            const std::vector<Id> uses = usedVars(s);

            for (const Id &u : uses) {
                if (u.isLocal() && defs.find(u.name) == defs.cend()) {
                    return Err(
                        "Local variable '" + u.name + "' not defined.",
                        span(u)
                    );
                }
            }

            if (!is<PhiStmt>(s)) {
                for (const Id &u : usedVars(s)) {
                    if (u.isLocal() && !sl.after(dt, defs.at(u.name))) {
                        return Err(
                            "Local variable '" + u.name + "' used "
                            "before definition (use must always come "
                            "strict after definition).",
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
                        // we are either defined in pred or some dominator.
                        if (!(pi == dl.block || dt.dominates(dl.block, pi))) {
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
            const std::optional<Err> e =
                _checkLocalDefBeforeUse(fn.body.value());
            if (e.has_value()) { return e.value(); }
        }
    }

    return std::nullopt;
}

}  // Anonymous namespace

namespace SVFIR {

bool isVoidType(const Type &t) { return is<VoidType>(t); }
bool isPtrType(const Type &t) { return is<PtrType>(t); }
bool isIntType(const Type &t) { return is<IntType>(t); }
bool isFloatType(const Type &t) { return is<FloatType>(t); }
bool isAggType(const Type &t) { return is<AggType>(t); }

bool operator==(const Type &x, const Type &y) {
    if (isVoidType(x) && isVoidType(y)) { return true; }
    else if (isPtrType(x) && isPtrType(y)) { return true; }
    else if (isFloatType(x) && isFloatType(y)) {
        return get<FloatType>(x).kind == get<FloatType>(y).kind;
    } else if (isIntType(x) && isIntType(y)) {
        return get<IntType>(x).kind == get<IntType>(y).kind;
    } else if (isAggType(x) && isAggType(y)) {
        return get<AggType>(x).slots == get<AggType>(y).slots;
    } else {
        insist(x.index() != y.index());
        return false;
    }
    unreachable();
}

bool operator!=(const Type &x, const Type &y) {
    return !(x == y);
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

    std::optional<Err> ve = checkVarargStmts(p);
    if (ve.has_value()) { return ve.value(); }

    std::optional<Err> be = checkUndeclaredBlocks(p);
    if (be.has_value()) { return be.value(); }

    std::optional<Err> pe = checkPhiStmtArgs(p);
    if (pe.has_value()) { return pe.value(); }

    std::optional<Err> dbue = checkLocalDefBeforeUse(p);
    if (dbue.has_value()) { return dbue.value(); }

    return Err("not implemented");
}

}  // namespace SVFIR
