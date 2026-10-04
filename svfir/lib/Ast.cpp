#include "Ast.h"

namespace SVFIR {

bool Function::isOpaque(void) const {
    return !body.has_value();
}

bool Variable::isOpaque(void) const {
    return !val.has_value();
}

bool TypeAlias::isOpaque(void) const {
    return !type.has_value();
}

}  // namespace SVFIR
