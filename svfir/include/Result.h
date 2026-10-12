#ifndef RESULT_H
#define RESULT_H

#include <variant>

namespace SVFIR {

template <typename T, typename E>
using Result = std::variant<T, E>;

}  // namespace SVFIR

#endif // RESULT_H
