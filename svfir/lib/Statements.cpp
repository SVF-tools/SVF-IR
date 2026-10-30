#include <algorithm>
#include <optional>

#include "Ast.h"
#include "Statements.h"
#include "Variant.h"

namespace {

}  // Anonymous namespace

namespace SVFIR {

std::optional<Id> introducedVar(const Statement &s) {
    std::optional<Id> id = std::nullopt;
    if (is<PhiStmt>(s)) {
        id.emplace(get<PhiStmt>(s).id);
    } else if (is<CallStmt>(s)) {
        const CallStmt c = get<CallStmt>(s);
        if (c.id.has_value()) { id.emplace(c.id.value()); }
    } else if (is<AllocStmt>(s)) {
        id.emplace(get<AllocStmt>(s).id);
    } else if (is<LoadStmt>(s)) {
        id.emplace(get<LoadStmt>(s).tid.id);
    } else if (is<FieldStmt>(s)) {
        id.emplace(get<FieldStmt>(s).id);
    } else if (is<BitCastStmt>(s)) {
        id.emplace(get<BitCastStmt>(s).id);
    } else if (is<ConvertStmt>(s)) {
        id.emplace(get<ConvertStmt>(s).id);
    } else if (is<BinaryOpStmt>(s)) {
        id.emplace(get<BinaryOpStmt>(s).id);
    } else if (is<UnaryOpStmt>(s)) {
        id.emplace(get<UnaryOpStmt>(s).id);
    } else if (is<VarargStmt>(s)) {
        id.emplace(get<VarargStmt>(s).tid.id);
    } else if (is<BlackholeStmt>(s)) {
        const BlackholeStmt b = get<BlackholeStmt>(s);
        if (b.tid.has_value()) { id.emplace(b.tid.value().id); }
    } else {
        // Nothing introduced.
        insist(
            is<StoreStmt>(s) ||
            is<RetStmt>(s) ||
            is<BrStmt>(s) ||
            is<BrifStmt>(s)
        );
    }

    return id;
}

// TODO: null pvals, ignore them?
std::vector<Val> usedVals(const Statement &s) {
    std::vector<Val> vals;
    if (is<PhiStmt>(s)) {
        for (const PhiStmt::Arg &arg : get<PhiStmt>(s).args) {
            vals.push_back(arg.val);
        }
    } else if (is<CallStmt>(s)) {
        const CallStmt &call = get<CallStmt>(s);
        if (is<Id>(call.callee)) { vals.push_back(get<Id>(call.callee)); }
        for (const Val &arg : call.args) { vals.push_back(arg); }
    } else if (is<RetStmt>(s)) {
        const RetStmt &ret = get<RetStmt>(s);
        if (ret.val.has_value()) { vals.push_back(ret.val.value()); }
    } else if (is<BrifStmt>(s)) { vals.push_back(get<BrifStmt>(s).cond); }
    else if (is<LoadStmt>(s)) {
        const LoadStmt &load = get<LoadStmt>(s);
        if (is<Id>(load.src)) { vals.push_back(get<Id>(load.src)); }
    } else if (is<StoreStmt>(s)) {
        const StoreStmt &store = get<StoreStmt>(s);
        vals.push_back(store.val);
        if (is<Id>(store.dst)) { vals.push_back(get<Id>(store.dst)); }
    } else if (is<FieldStmt>(s)) {
        const FieldStmt &field = get<FieldStmt>(s);
        if (is<Id>(field.src)) { vals.push_back(get<Id>(field.src)); }
        if (is<Id>(field.index)) { vals.push_back(get<Id>(field.index)); }
    } else if (is<BitCastStmt>(s)) { vals.push_back(get<BitCastStmt>(s).val); }
    else if (is<ConvertStmt>(s)) { vals.push_back(get<ConvertStmt>(s).val); }
    else if (is<BinaryOpStmt>(s)) {
        const BinaryOpStmt &binop = get<BinaryOpStmt>(s);
        vals.push_back(binop.left);
        vals.push_back(binop.right);
    } else if (is<UnaryOpStmt>(s)) { vals.push_back(get<UnaryOpStmt>(s).val); }
    else if (is<VarargStmt>(s)) {
        const VarargStmt &vararg = get<VarargStmt>(s);
        if (is<Id>(vararg.index)) {
            vals.push_back(Val(get<Id>(vararg.index)));
        }
    } else {
        // Nothing used.
        insist(is<AllocStmt>(s) || is<BrStmt>(s) || is<BlackholeStmt>(s));
    }

    return vals;
}

std::vector<Id> usedVars(const Statement &s) {
    std::vector<Id> ids;
    for (const Val &val : usedVals(s)) {
        // Vectors small enough for performance to be a non-issue.
        if (is<Id>(val)) { ids.push_back(get<Id>(val)); }
    }
    return ids;
}

std::vector<Id> usedBlocks(const Statement &s) {
    std::vector<Id> blocks;

    if (is<BrStmt>(s)) {
        const BrStmt &br = get<BrStmt>(s);
        blocks.push_back(br.target);
    } else if (is<BrifStmt>(s)) {
        const BrifStmt &brif = get<BrifStmt>(s);
        blocks.push_back(brif.ifTarget);
        blocks.push_back(brif.elseTarget);
    } else if (is<PhiStmt>(s)) {
        const PhiStmt &phi = get<PhiStmt>(s);
        for (const PhiStmt::Arg &arg : phi.args) { blocks.push_back(arg.pred); }
    } else {
        // No blocks referred to.
        insist(
            is<CallStmt>(s) || is<RetStmt>(s) || is<AllocStmt>(s) ||
            is<StoreStmt>(s) || is<LoadStmt>(s) || is<FieldStmt>(s) ||
            is<BitCastStmt>(s) || is<ConvertStmt>(s) || is<BinaryOpStmt>(s) ||
            is<UnaryOpStmt>(s) || is<VarargStmt>(s) || is<BlackholeStmt>(s)
        );
    }

    return blocks;
}

}  // namespace SVFIR
