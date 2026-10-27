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
        insist(is<RetStmt>(s) || is<BrStmt>(s) || is<BrifStmt>(s));
    }

    return id;
}

}  // namespace SVFIR
