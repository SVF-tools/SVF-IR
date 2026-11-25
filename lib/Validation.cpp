#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "SVFIR/Ast.h"
#include "SVFIR/AstHelpers.h"
#include "SVFIR/Contracts.h"
#include "SVFIR/ControlFlowGraph.h"
#include "SVFIR/DominatorTree.h"
#include "SVFIR/Err.h"
#include "SVFIR/Index.h"
#include "SVFIR/Pretty.h"
#include "SVFIR/Result.h"
#include "SVFIR/Statements.h"
#include "SVFIR/TypedProgram.h"
#include "SVFIR/Validation.h"
#include "SVFIR/Variant.h"

namespace {

using namespace SVFIR;

// checkDuplicateDecs //////////////////////////////////////////////////////////
std::optional<Err> _checkDuplicateGlobalDecs(
    const std::vector<GlobalVar> &vs,
    const std::vector<Function> &fns
) {
    std::unordered_set<Name> names;
    for (const GlobalVar &v : vs) {
        const Name &name = v.id.name;
        if (!names.insert(name).second) {
            return Err(
                "Global variable name '" + name + "' previously declared",
                span(v.id)
            );
        }
    }
    for (const Function &fn : fns) {
        const Name &name = fn.id.name;
        if (!names.insert(name).second) {
            return Err(
                "Function name '" + name + "' previously declared", span(fn.id)
            );
        }
    }
    return std::nullopt;
}

std::optional<Err> _checkDuplicateLocalDecs(const Function &fn) {
    std::unordered_set<Name> names;
    for (const auto &p : fn.params) {
        const Name &name = p.id.name;
        if (!names.insert(name).second) {
            return Err(
                "Parameter '" + name + "' previously declared.", span(p.id)
            );
        }
    }

    if (fn.body.has_value()) {
        for (const BasicBlock &bb : fn.body.value()) {
            for (const Statement &s : bb.stmts) {
                const std::optional<Id> &id = introducedVar(s);
                if (id.has_value()) {
                    const Name &name = id.value().name;
                    if (!names.insert(name).second) {
                        return Err(
                            "Variable '" + name + "' preivously declared.",
                            span(id.value())
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
        for (const BasicBlock &bb : fn.body.value()) {
            const Name &name = bb.id.name;
            if (names.find(name) != names.cend()) {
                return Err(
                    "Block '" + name + "' previously declared.",
                    span(bb.id)
                );
            } else { names.insert(name); }
        }
    }

    return std::nullopt;
}

// checkPhiStmtArgs ////////////////////////////////////////////////////////////
std::optional<Err> _checkPhiStmtArgs(const FunctionBody &bbs) {
    const ControlFlowGraph &cfg = ControlFlowGraph(bbs);
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
                for (const Index pi : cfg.nodes[i].inEdges) {
                    preds.insert(bbs[pi].id.name);
                }

                std::unordered_set<Name> seen;
                for (const PhiStmt::Arg &arg : phi.args) {
                    const Name &argName = arg.pred.name;
                    if (seen.find(argName) != seen.cend()) {
                        return Err(
                            "Duplicate argument for predecessor block '" +
                            argName + "' in phi statement '" + pretty(phi) +
                            "'.",
                            arg.span
                        );
                    } else { seen.insert(argName); }
                    if (!preds.erase(argName)) {
                        return Err(
                            "Argument block '" + argName + "' is not a "
                            "predecessor block in phi statement '" +
                            pretty(phi) + "'.",
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
                        "No corresponding argument for predecessor block" +
                        (preds.size() == 1 ? " " + uncovered : "s " + uncovered)
                        + " in phi statement '" + pretty(phi) + "'.",
                        span(phi)
                    );
                }
            }
        }
    }

    return std::nullopt;
}

// checkLocalDefBeforeUse //////////////////////////////////////////////////////
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
            const std::optional<Id> &var = introducedVar(s);
            if (var.has_value()) {
                defs.insert({var.value().name, StmtLoc(bbi, si)});
            }
        }
    }
    return defs;
}

std::optional<Err> _checkLocalDefBeforeUse(
    const FunctionBody &bbs,
    const std::unordered_set<Name> &params
) {
    const std::unordered_map<Name, StmtLoc> &defs = _localDefSites(bbs);
    const ControlFlowGraph &cfg = ControlFlowGraph(bbs);
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    for (Index bbi = 0; bbi < bbs.size(); ++bbi) {
        const BasicBlock &bb = bbs[bbi];
        for (Index si = 0; si < bb.stmts.size(); ++si) {
            const Statement &s = bb.stmts[si];
            const StmtLoc &sl = StmtLoc(bbi, si);
            const std::vector<Id> &uses = directlyUsedVars(s);

            if (!is<PhiStmt>(s)) {
                for (const Id &u : uses) {
                    if (
                        u.isLocal() &&
                        params.find(u.name) == params.cend() &&
                        (
                            defs.find(u.name) == defs.cend() ||
                            !sl.after(dt, defs.at(u.name))
                        )
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
                for (const Index pred : cfg.nodes[bbi].inEdges) {
                    preds[cfg.nodes[pred].name] = pred;
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

// checkConstantTypes //////////////////////////////////////////////////////////
Err _typedConstMismatchErr(const TypedConstant &tc) {
    std::string expType;
    if (is<IntConstant>(tc.constant)) { expType = "integer type"; }
    else if (is<FloatConstant>(tc.constant)) { expType = "float type"; }
    else if (is<NullConstant>(tc.constant)) { expType = "pointer type"; }
    else if (is<SeqConstant>(tc.constant)) { expType = "aggregation type"; }
    else { unreachable(); }

    return Err(
        "Constant typed incorrectly. Declared as '" + pretty(tc.type) + "' but "
        "expected " + expType + " for '" + pretty(tc.constant) + "' in '" +
        pretty(tc) + "'.",
        span(tc)
    );
}

std::optional<Err> _checkConstantType(const TypedConstant &tc) {
    if (
        is<VoidType>(tc.type) ||
        (is<IntType>(tc.type) && !is<IntConstant>(tc.constant)) ||
        (is<FloatType>(tc.type) && !is<FloatConstant>(tc.constant)) ||
        (is<PtrType>(tc.type) && !is<NullConstant>(tc.constant)) ||
        (is<AggType>(tc.type) && !is<SeqConstant>(tc.constant))
    ) {
        return _typedConstMismatchErr(tc);
    }

    if (is<SeqConstant>(tc.constant)) {
        // We check is<AggType> above.
        const AggType &at = get<AggType>(tc.type);
        const SeqConstant &sc = get<SeqConstant>(tc.constant);
        if (sc.vals.size() != at.slots) {
            return Err(
                "Declared sequence constant to have " +
                std::to_string(at.slots) + " slots but in actuality has " +
                std::to_string(sc.vals.size()) + " slots in '" +
                pretty(tc) + "'.",
                span(tc)
            );
        }

        for (const Val &v : get<SeqConstant>(tc.constant).vals) {
            if (is<TypedConstant>(v)) {
                const std::optional<Err> &ce =
                    _checkConstantType(get<TypedConstant>(v));
                if (ce.has_value()) { return ce.value(); }
            }
        }
    }

    return std::nullopt;
}

}  // Anonymous namespace

namespace SVFIR {

std::optional<Err> checkDuplicateDecs(const Program &p) {
    const std::optional<Err> &gs = _checkDuplicateGlobalDecs(
        p.globalVars, p.functions
    );
    if (gs.has_value()) { return gs.value(); }

    for (const Function &fn : p.functions) {
        const std::optional<Err> &ls = _checkDuplicateLocalDecs(fn);
        if (ls.has_value()) { return ls.value(); }

        const std::optional<Err> &bs = _checkDuplicateBlockDecs(fn);
        if (bs.has_value()) { return bs.value(); }
    }

    return std::nullopt;
}

std::optional<Err> checkTerminatingStmts(const Program &p) {
    for (const Function &fn : p.functions) {
        if (fn.body.has_value()) {
            for (const BasicBlock &bb : fn.body.value()) {
                for (auto it = bb.stmts.cbegin(); it != bb.stmts.cend(); ++it) {
                    if (it + 1 != bb.stmts.cend()) {
                        // Not at the end? Must not terminate.
                        if (isTerminatingStmt(*it)) {
                            return Err(
                                "Terminating statement '" +
                                pretty(*it) +
                                "' not final statement of basic block '" +
                                bb.id.name + "'.",
                                span(*it)
                            );
                        }
                    } else {
                        // At the end? Must terminate.
                        if (!isTerminatingStmt(*it)) {
                            return Err(
                                "Non-terminating statement '" +
                                pretty(*it) +
                                "' cannot be final statement of basic block '" +
                                bb.id.name + "'.",
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

std::optional<Err> checkVarargStmts(const Program &p) {
    for (const Function &fn : p.functions) {
        if (!fn.vararg && fn.body.has_value()) {
            for (const BasicBlock &bb : fn.body.value()) {
                for (const Statement &s : bb.stmts) {
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

std::optional<Err> checkUndeclaredBlocks(const Program &p) {
    for (const Function &fn : p.functions) {
        if (fn.body.has_value()) {
            std::unordered_set<Name> blocks;
            for (const BasicBlock &bb : fn.body.value()) {
                blocks.insert(bb.id.name);
            }

            for (const BasicBlock &bb : fn.body.value()) {
                for (const Statement &s : bb.stmts) {
                    for (const Id &blk : usedBlocks(s)) {
                        if (blocks.find(blk.name) == blocks.cend()) {
                            return Err(
                                "Block '" + blk.name + "' undeclared.",
                                span(blk)
                            );
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkPhiStmtArgs(const Program &p) {
    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            const std::optional<Err> &pe = _checkPhiStmtArgs(fn.body.value());
            if (pe.has_value()) { return pe.value(); }
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

            const std::optional<Err> &e =
                _checkLocalDefBeforeUse(fn.body.value(), params);
            if (e.has_value()) { return e.value(); }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkUndeclaredGlobals(const Program &p) {
    std::unordered_set<Name> globals;
    for (const GlobalVar &gv : p.globalVars) { globals.insert(gv.id.name); }
    for (const Function &fn : p.functions) { globals.insert(fn.id.name); }

    for (const GlobalVar &gv : p.globalVars) {
        if (!gv.isOpaque()) {
            if (is<Id>(gv.val.value())) {
                const Id &vi = get<Id>(gv.val.value());
                // Global-ness encoded in grammar.
                if (globals.find(vi.name) == globals.cend()) {
                    return Err(
                        "Global '" + vi.name + "' undeclared.", span(vi)
                    );
                }
            }
        }
    }

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
                                "Global '" + u.name + "' undeclared.", span(u)
                            );
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> checkConstantTypes(const Program &p) {
    for (const GlobalVar &gv : p.globalVars) {
        if (!gv.isOpaque() && is<TypedConstant>(gv.val.value())) {
            const TypedConstant &tc = get<TypedConstant>(gv.val.value());
            const std::optional<Err> &ce = _checkConstantType(tc);
            if (ce.has_value()) { return ce.value(); }
        }
    }

    for (const Function &fn : p.functions) {
        if (!fn.isOpaque()) {
            for (const BasicBlock &bb : fn.body.value()) {
                for (const Statement &s : bb.stmts) {
                    const std::vector<Val> &uvs = usedVals(s);
                    for (const Val &uv : uvs) {
                        if (is<TypedConstant>(uv)) {
                            const std::optional<Err> &ce =
                                _checkConstantType(get<TypedConstant>(uv));
                            if (ce.has_value()) { return ce.value(); }
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Err> analyse(
    const Program &p,
    const std::vector<CheckAnalysis> &analyses
) {
    for (const CheckAnalysis &analyse : analyses) {
        const std::optional<Err> &e = analyse(p);
        if (e.has_value()) { return e.value(); }
    }

    return std::nullopt;
}

}  // namespace SVFIR
