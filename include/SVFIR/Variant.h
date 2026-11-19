#ifndef VARIANT_H
#define VARIANT_H

#include <variant>

namespace SVFIR {

/// Alias to std::holds_alternative.
template<class T, class... Types>
constexpr bool is(const std::variant<Types...> &v) noexcept {
    return std::holds_alternative<T>(v);
}

using std::get;

}  // namespace SVFIR

#endif  // VAIRANT_H
