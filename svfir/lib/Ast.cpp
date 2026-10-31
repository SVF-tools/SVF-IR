#include "Ast.h"

namespace SVFIR {

bool Function::isOpaque(void) const {
    return !body.has_value();
}

std::vector<Type> Function::paramTypes(void) const {
    std::vector<Type> types;
    for (const Param &p : params) { types.push_back(p.type); }
    return types;
}

bool GlobalVar::isOpaque(void) const {
    return !val.has_value();
}

bool Id::isLocal(void) const { return name[0] == '%'; }
bool Id::isGlobal(void) const { return name[0] == '@'; }
bool Id::isBlock(void) const { return name[0] == '!'; }

bool operator==(const Type &x, const Type &y) {
    if (is<VoidType>(x) && is<VoidType>(y)) { return true; }
    else if (is<PtrType>(x) && is<PtrType>(y)) { return true; }
    else if (is<FloatType>(x) && is<FloatType>(y)) {
        return get<FloatType>(x).kind == get<FloatType>(y).kind;
    } else if (is<IntType>(x) && is<IntType>(y)) {
        return get<IntType>(x).kind == get<IntType>(y).kind;
    } else if (is<AggType>(x) && is<AggType>(y)) {
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

}  // namespace SVFIR
