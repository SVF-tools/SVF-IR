#include "Ast.h"

namespace SVFIR {

bool Function::isOpaque(void) const {
    return !body.has_value();
}

bool Variable::isOpaque(void) const {
    return !val.has_value();
}

bool Id::isLocal(void) const { return name[0] == '%'; }
bool Id::isGlobal(void) const { return name[0] == '@'; }
bool Id::isBlock(void) const { return name[0] == '!'; }

}  // namespace SVFIR
