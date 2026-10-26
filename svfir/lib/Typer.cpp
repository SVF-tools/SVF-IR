#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "Contracts.h"
#include "ControlFlowGraph.h"
#include "Ops.h"
#include "Err.h"
#include "Result.h"
#include "Statements.h"
#include "TypedProgram.h"

namespace {

using namespace SVFIR;

////////////////////////////////////////////////////////////////////////////////
/// PASS: checkDuplicateDecs
/// DEPS: --
/// Raise an error if duplicate IDs are declared. This comes down to:
///  * Duplicately named aliases in the (types ...) block.
///  * Duplicately named functions and variables (same namespace).
///  * Duplicately named local variables within a function, including paramters.
///  * Duplicately named blocks within a function.
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> _checkDuplicateTypeDecs(const std::vector<TypeAlias> &tas) {
    std::unordered_set<Name> names;
    for (const auto &ta : tas) {
        const Name name = ta.id.name;
        if (names.insert(name).second) {
            return Err("Type alias '" + name + "' declared twice", ta.span);
        }
    }
    return std::nullopt;
}

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
                const std::optional<Id> id = introducedId(s);
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
    const std::optional<Err> ts = _checkDuplicateTypeDecs(p.types);
    if (ts.has_value()) { return ts.value(); }

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
                    if (std::holds_alternative<VarargStmt>(s)) {
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
///  * phi instructions referring to non-existent blocks in operands.
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
            if (std::holds_alternative<BrStmt>(s)) {
                const BrStmt &br = std::get<BrStmt>(s);
                if (blocks.find(br.target.name) == blocks.cend()) {
                    return _nonExistentBlock(br.target.name, br.target.span);
                }
            } else if (std::holds_alternative<BrifStmt>(s)) {
                const BrifStmt &brif = std::get<BrifStmt>(s);
                if (blocks.find(brif.ifTarget.name) == blocks.cend()) {
                    return _nonExistentBlock(
                        brif.ifTarget.name, brif.ifTarget.span
                    );
                }
                if (blocks.find(brif.elseTarget.name) == blocks.cend()) {
                    return _nonExistentBlock(
                        brif.elseTarget.name, brif.elseTarget.span
                    );
                }
            } else if (std::holds_alternative<PhiStmt>(s)) {
                const PhiStmt &phi = std::get<PhiStmt>(s);
                for (const PhiStmt::Operand &op : phi.args) {
                    const Id &db = op.definingBlock;
                    if (blocks.find(db.name) == blocks.end()) {
                        return _nonExistentBlock(db.name, db.span);
                    }
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

}  // Anonymous namespace

namespace SVFIR {

bool isVoidType(const Type &t) { return std::holds_alternative<VoidType>(t); }
bool isPtrType(const Type &t) { return std::holds_alternative<PtrType>(t); }
bool isIntType(const Type &t) { return std::holds_alternative<IntType>(t); }
bool isFloatType(const Type &t) { return std::holds_alternative<FloatType>(t); }
bool isAggType(const Type &t) { return std::holds_alternative<AggType>(t); }
bool isTypeId(const Type &t) { return std::holds_alternative<TypeId>(t); }
bool isOpaqueType(const Type &t) {
    return std::holds_alternative<OpaqueType>(t);
}

bool operator==(const Type &x, const Type &y) {
    if (isVoidType(x) && isVoidType(y)) { return true; }
    else if (isPtrType(x) && isPtrType(y)) { return true; }
    else if (isFloatType(x) && isFloatType(y)) {
        return std::get<FloatType>(x).kind == std::get<FloatType>(y).kind;
    } else if (isIntType(x) && isIntType(y)) {
        return std::get<IntType>(x).kind == std::get<IntType>(y).kind;
    } else if (isAggType(x) && isAggType(y)) {
        return std::get<AggType>(x).slots == std::get<AggType>(y).slots;
    } else if (isTypeId(x) && isTypeId(y)) {
        return std::get<TypeId>(x).name == std::get<TypeId>(y).name;
    } else if (isOpaqueType(x) && isOpaqueType(y)) { return false; }
    else if (isVarargType(x) && isVarargType(y)) { return true; }
    else {
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
        std::holds_alternative<BrStmt>(stmt) ||
        std::holds_alternative<BrifStmt>(stmt) ||
        std::holds_alternative<RetStmt>(stmt)
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

    return Err("not implemented");
}

}  // namespace SVFIR
