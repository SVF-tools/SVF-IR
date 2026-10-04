#include <string>

#include "Span.h"

namespace SVFIR {
    std::string Loc::toString(void) const {
        return "<" + std::to_string(_line) + ", " + std::to_string(_col) + ">";
    }

    std::string Span::toString(void) const {
        return "[" + start.toString() + ", " + end.toString() + ")";
    }

    bool Loc::operator==(const Loc &y) const {
        return this->_line == y._line && this->_col == y._col;
    }

    bool Loc::operator!=(const Loc &y) const {
        return !(*this == y);
    }
}  // namespace SVFIR
