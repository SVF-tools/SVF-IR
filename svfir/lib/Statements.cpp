#include <optional>

#include "Statements.h"
#include "Ast.h"

namespace {

}  // Anonymous namespace

namespace SVFIR {

bool isRet(const Statement &s) { return std::holds_alternative<RetStmt>(s); }

std::optional<Id> introducedVar(const Statement &s) {
    std::optional<Id> id = std::nullopt;
    if (std::holds_alternative<PhiStmt>(s)) {
        id.emplace(std::get<PhiStmt>(s).id);
    } else if (std::holds_alternative<CallStmt>(s)) {
        const CallStmt c = std::get<CallStmt>(s);
        if (c.id.has_value()) { id.emplace(c.id.value()); }
    } else if (std::holds_alternative<CmpStmt>(s)) {
        id.emplace(std::get<CmpStmt>(s).id);
    } else if (std::holds_alternative<AllocStmt>(s)) {
        id.emplace(std::get<AllocStmt>(s).id);
    } else if (std::holds_alternative<LoadStmt>(s)) {
        id.emplace(std::get<LoadStmt>(s).tid.id);
    } else if (std::holds_alternative<FieldStmt>(s)) {
        id.emplace(std::get<FieldStmt>(s).id);
    } else if (std::holds_alternative<BitCastStmt>(s)) {
        id.emplace(std::get<BitCastStmt>(s).id);
    } else if (std::holds_alternative<ConvertStmt>(s)) {
        id.emplace(std::get<ConvertStmt>(s).id);
    } else if (std::holds_alternative<BinaryOpStmt>(s)) {
        id.emplace(std::get<BinaryOpStmt>(s).id);
    } else if (std::holds_alternative<UnaryOpStmt>(s)) {
        id.emplace(std::get<UnaryOpStmt>(s).id);
    } else if (std::holds_alternative<AssignStmt>(s)) {
        id.emplace(std::get<AssignStmt>(s).id);
    } else if (std::holds_alternative<VarargStmt>(s)) {
        id.emplace(std::get<VarargStmt>(s).tid.id);
    } else if (std::holds_alternative<BlackholeStmt>(s)) {
        id.emplace(std::get<BlackholeStmt>(s).tid.id);
    } else {
        // Nothing introduced.
        insist(
            std::holds_alternative<RetStmt>(s) ||
            std::holds_alternative<BrStmt>(s) ||
            std::holds_alternative<BrifStmt>(s)
        );
    }

    return id;
}

}  // namespace SVFIR
