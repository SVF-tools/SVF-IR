#include <variant>

#include "Contracts.h"
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

bool Err::operator==(const Err &e) const {
    return this->msg == e.msg && this->span == e.span;
}

};  // namespace SVFIR
