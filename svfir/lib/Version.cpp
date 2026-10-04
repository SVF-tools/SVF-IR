#include "Version.h"

namespace SVFIR {

bool Version::compatibleWith(const Version &y) const {
    return this->major == y.major && this->minor <= y.minor;
}

bool Version::operator==(const Version &y) {
    return this->major == y.major && this->minor == y.minor;
}

bool Version::operator!=(const Version &y) {
    return !(*this == y);
}

std::string Version::toString(void) const {
    return std::to_string(major) + "." + std::to_string(minor);
}

bool Version::operator<(const Version &y) {
    return (
        this->major < y.major ||
        (this->major == y.major && this->minor < y.minor)
    );
}

bool Version::operator<=(const Version &y) {
    return *this == y || *this < y;
}

bool Version::operator>(const Version &y) {
    return *this != y && !(*this < y);
}

bool Version::operator>=(const Version &y) {
    return !(*this < y);
}

}  // namespace SVFIR
