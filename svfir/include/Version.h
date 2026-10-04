#ifndef VERSION_H
#define VERSION_H

#include <cstdint>
#include <string>

namespace SVFIR {

/// Version number for SVFIR.
class Version {
    const uint16_t minor;
    const uint16_t major;

public:
    Version(
        const uint16_t minor,
        const uint16_t major
    ) : minor(minor), major(major)
    { }

    /// Version x is compatible with version y when they have the same major
    /// version and x's minor version is the same or lesser than that of y.
    /// When x.compatibleWith(y), a program of Version x can be handled by an
    /// implementation of version y.
    bool compatibleWith(const Version &y) const;

    bool operator==(const Version &y);
    bool operator!=(const Version &y);

    std::string toString(void) const;

    // TODO: we may not need these, actually.
    /// x < y when the major version is of x is less than that of y or the
    /// minor version of x is less than that of y when both have the same
    /// major version.
    bool operator<(const Version &y);
    bool operator<=(const Version &y);
    bool operator>(const Version &y);
    bool operator>=(const Version &y);
};

}  // namespace SVFIR

#endif // VERSION_H
