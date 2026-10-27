#include <optional>

#include "Ast.h"
#include "Statements.h"
#include "Variant.h"

namespace {

}  // Anonymous namespace

namespace SVFIR {

bool isRet(const Statement &s) { return is<RetStmt>(s); }

std::optional<Id> introducedVar(const Statement &s) {
    std::optional<Id> id = std::nullopt;
    if (is<PhiStmt>(s)) {
        id.emplace(get<PhiStmt>(s).id);
    } else if (is<CallStmt>(s)) {
        const CallStmt c = get<CallStmt>(s);
        if (c.id.has_value()) { id.emplace(c.id.value()); }
    } else if (is<CmpStmt>(s)) {
        id.emplace(get<CmpStmt>(s).id);
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
    } else if (is<AssignStmt>(s)) {
        id.emplace(get<AssignStmt>(s).id);
    } else if (is<VarargStmt>(s)) {
        id.emplace(get<VarargStmt>(s).tid.id);
    } else if (is<BlackholeStmt>(s)) {
        id.emplace(get<BlackholeStmt>(s).tid.id);
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

std::vector<Id> usedVars(const Statement &s) {
    std::vector<Id> ids;
    if (is<PhiStmt>(s)) {
        const PhiStmt &phi = get<PhiStmt>(s);
        for (const PhiStmt::Arg &arg : phi.args) {
            if (is<Id>(arg.val)) { ids.push_back(get<Id>(arg.val)); }
        }
    } else if (is<CallStmt>(s)) {
        const CallStmt &call = get<CallStmt>(s);
        if (is<Id>(call.callee)) { ids.push_back(get<Id>(call.callee)); }
        for (const Val &arg : call.args) {
            if (is<Id>(arg)) { ids.push_back(get<Id>(arg)); }
        }
    } else if (is<RetStmt>(s)) {
        const RetStmt &ret = get<RetStmt>(s);
        if (ret.val.has_value() && is<Id>(ret.val.value())) {
            ids.push_back(get<Id>(ret.val.value()));
        }
    } else if (is<BrifStmt>(s)) {
        const BrifStmt &brif = get<BrifStmt>(s);
        if (is<Id>(brif.cond)) { ids.push_back(get<Id>(brif.cond)); }
    } else if (is<CmpStmt>(s)) {
        const CmpStmt &cmp = get<CmpStmt>(s);
        if (is<Id>(cmp.left)) { ids.push_back(get<Id>(cmp.left)); }
        if (is<Id>(cmp.right)) { ids.push_back(get<Id>(cmp.right)); }
    } else if (is<LoadStmt>(s)) {
        const LoadStmt &load = get<LoadStmt>(s);
        if (is<Id>(load.src)) { ids.push_back(get<Id>(load.src)); }
    } else if (is<StoreStmt>(s)) {
        const StoreStmt &store = get<StoreStmt>(s);
        if (is<Id>(store.val)) { ids.push_back(get<Id>(store.val)); }
        if (is<Id>(store.dst)) { ids.push_back(get<Id>(store.dst)); }
    } else if (is<FieldStmt>(s)) {
        const FieldStmt &field = get<FieldStmt>(s);
        if (is<Id>(field.src)) { ids.push_back(get<Id>(field.src)); }
        if (is<Id>(field.index)) { ids.push_back(get<Id>(field.index)); }
    } else if (is<BitCastStmt>(s)) {
        const BitCastStmt &bitcast = get<BitCastStmt>(s);
        if (is<Id>(bitcast.val)) { ids.push_back(get<Id>(bitcast.val)); }
    } else if (is<ConvertStmt>(s)) {
        const ConvertStmt &convert = get<ConvertStmt>(s);
        if (is<Id>(convert.val)) { ids.push_back(get<Id>(convert.val)); }
    } else if (is<BinaryOpStmt>(s)) {
        const BinaryOpStmt &binop = get<BinaryOpStmt>(s);
        if (is<Id>(binop.left)) { ids.push_back(get<Id>(binop.left)); }
        if (is<Id>(binop.right)) { ids.push_back(get<Id>(binop.right)); }
    } else if (is<UnaryOpStmt>(s)) {
        const UnaryOpStmt &unop = get<UnaryOpStmt>(s);
        if (is<Id>(unop.val)) { ids.push_back(get<Id>(unop.val)); }
    } else if (is<AssignStmt>(s)) {
        const AssignStmt &assign = get<AssignStmt>(s);
        if (is<Id>(assign.val)) { ids.push_back(get<Id>(assign.val)); }
    } else if (is<VarargStmt>(s)) {
        const VarargStmt &vararg = get<VarargStmt>(s);
        if (is<Id>(vararg.index)) { ids.push_back(get<Id>(vararg.index)); }
    } else {
        // Nothing used.
        insist(is<AllocStmt>(s) || is<BrStmt>(s) || is<BlackholeStmt>(s));
    }

    return ids;
}

}  // namespace SVFIR
