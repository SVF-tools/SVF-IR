#include "Ast.h"

namespace SVFIR {

bool Function::isDefinition(void) const {
    return body.has_value();
}

bool Variable::isDefinition(void) const {
    return val.has_value();
}

bool TypeAlias::isOpaque(void) const {
    return type.has_value();
}

}  // namespace SVFIR
