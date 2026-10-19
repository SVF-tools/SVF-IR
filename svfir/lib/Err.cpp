#include <variant>

#include "Abort.h"
#include "Err.h"

namespace SVFIR {

std::string Err::toString(void) const {
    if (span.has_value()) {
        return span.value().toString() + " -> " + msg;
    } else {
        return msg;
    }
    unreachable();
}

};  // namespace SVFIR
