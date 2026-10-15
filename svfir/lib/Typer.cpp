#include <cassert>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "Ops.h"
#include "Err.h"
#include "Result.h"
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
////////////////////////////////////////////////////////////////////////////////

std::optional<Err> _checkDuplicateTypeDecs(const std::vector<TypeAlias> &tas) {
    std::unordered_set<Id> ids;
    for (const auto &ta : tas) {
        const Id id = ta.id.id;
        if (ids.insert(id).second) {
            return Err("Type alias '" + id + "' declared twice", ta.span);
        }
    }
    return std::nullopt;
}

std::optional<Err> _checkDuplicateGlobalDecs(
    const std::vector<Variable> &vs,
    const std::vector<Function> &fns
) {
    std::unordered_set<Id> ids;
    for (const auto &v : vs) {
        const Id id = v.id.id;
        if (ids.insert(id).second) {
            return Err("Variable name '" + id + "' declared twice", v.span);
        }
    }
    for (const auto &fn : fns) {
        const Id id = fn.id.id;
        if (ids.insert(id).second) {
            return Err("Function name '" + id + "' declared twice", fn.span);
        }
    }
    return std::nullopt;
}

std::optional<Err> _checkDuplicateLocalDecs(const Function &fn) {
    std::unordered_set<Id> ids;
    for (const auto &p : fn.params) {
        const Id id = p.id.id;
        if (ids.insert(id).second) {
            return Err("Parameter '" + id + "' declared twice", fn.span);
        }
    }

    if (fn.body.has_value()) {
        for (const auto &bb : fn.body.value()) {
            for (const auto &s : bb.stmts) {
                std::optional<Id> id;
                if (std::holds_alternative<PhiStmt>(s)) {
                    id.emplace(std::get<PhiStmt>(s).id.id);
                } else if (std::holds_alternative<CallStmt>(s)) {
                    const CallStmt c = std::get<CallStmt>(s);
                    if (c.id.has_value()) {
                        id.emplace(c.id.value().id);
                    }
                } else if (std::holds_alternative<CmpStmt>(s)) {
                    id.emplace(std::get<CmpStmt>(s).id.id);
                } else if (std::holds_alternative<AllocStmt>(s)) {
                    id.emplace(std::get<AllocStmt>(s).id.id);
                } else if (std::holds_alternative<LoadStmt>(s)) {
                    id.emplace(std::get<LoadStmt>(s).tid.id.id);
                } else if (std::holds_alternative<FieldStmt>(s)) {
                    id.emplace(std::get<FieldStmt>(s).id.id);
                } else if (std::holds_alternative<ReinterpretStmt>(s)) {
                    id.emplace(std::get<ReinterpretStmt>(s).id.id);
                } else if (std::holds_alternative<ConvertStmt>(s)) {
                    id.emplace(std::get<ConvertStmt>(s).id.id);
                } else if (std::holds_alternative<IntTruncStmt>(s)) {
                    id.emplace(std::get<IntTruncStmt>(s).id.id);
                } else if (std::holds_alternative<IntExtStmt>(s)) {
                    id.emplace(std::get<IntExtStmt>(s).id.id);
                } else if (std::holds_alternative<BinaryOpStmt>(s)) {
                    id.emplace(std::get<BinaryOpStmt>(s).id.id);
                } else if (std::holds_alternative<UnaryOpStmt>(s)) {
                    id.emplace(std::get<UnaryOpStmt>(s).id.id);
                } else if (std::holds_alternative<AssignStmt>(s)) {
                    id.emplace(std::get<AssignStmt>(s).id.id);
                } else if (std::holds_alternative<VarargStmt>(s)) {
                    id.emplace(std::get<VarargStmt>(s).tid.id.id);
                } else if (std::holds_alternative<BlackholeStmt>(s)) {
                    id.emplace(std::get<BlackholeStmt>(s).tid.id.id);
                } else {
                    assert(
                        std::holds_alternative<RetStmt>(s) ||
                        std::holds_alternative<BrStmt>(s) ||
                        std::holds_alternative<BrifStmt>(s)
                    );
                }

                if (id.has_value()) {
                    if (ids.insert(id.value()).second) {
                        return Err(
                            "Variable '" + id.value() + "' declared twice",
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
/// PASS: checkTerminatingStmts
/// DEPS: --
/// Raise an error if either:
///  * A basic block ends with a non-terminating statement.
///  * A terminating statement appears anywhere but as the last statement of a
///    basic block.
///
/// Terminating statements are br, brif, and ret.
////////////////////////////////////////////////////////////////////////////////

bool _isTerminatingStmt(const Statement &stmt) {
    return (
        std::holds_alternative<BrStmt>(stmt) ||
        std::holds_alternative<BrifStmt>(stmt) ||
        std::holds_alternative<RetStmt>(stmt)
    );
}

std::optional<Err> checkTerminatingStmts(const Program &p) {
    for (const auto &fn : p.functions) {
        if (fn.body.has_value()) {
            for (const auto &bb : fn.body.value()) {
                for (auto it = bb.stmts.cbegin(); it != bb.stmts.cend(); ++it) {
                    if (it + 1 != bb.stmts.cend()) {
                        // Not at the end? Must not terminate.
                        if (_isTerminatingStmt(*it)) {
                            return Err(
                                "Terminating statement '" +
                                pretty(*it) +
                                "' not final statement of basic block",
                                span(*it)
                            );
                        }
                    } else {
                        // At the end? Must terminate.
                        if (!_isTerminatingStmt(*it)) {
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
        return std::get<TypeId>(x).id == std::get<TypeId>(y).id;
    } else if (isOpaqueType(x) && isOpaqueType(y)) { return false; }
    else if (isVarargType(x) && isVarargType(y)) { return true; }
    else {
        assert(x.index() != y.index());
        return false;
    }
    assert(false);
}

bool operator!=(const Type &x, const Type &y) {
    return !(x == y);
}

std::optional<Err> checkDuplicateDecs(const Program &p) {
    return std::nullopt;
}

Result<TypedProgram, Err> typeProgram(const Program &p) {
    std::optional<Err> de = checkDuplicateDecs(p);
    if (de.has_value()) { return de.value(); }

    std::optional<Err> te = checkTerminatingStmts(p);
    if (te.has_value()) { return te.value(); }

    std::optional<Err> ve = checkVarargStmts(p);
    if (ve.has_value()) { return ve.value(); }

    return Err("not implemented");
}

}  // namespace SVFIR
