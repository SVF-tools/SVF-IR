#ifndef RESULT_H
#define RESULT_H

#include <variant>

namespace SVFIR {

template <typename T, typename E>
using Result = std::variant<T, E>;

// Special case of Result<T, ErrMsg>:

using ErrMsg = std::string;

template <typename T>
bool isErr(const Result<T, ErrMsg> res) {
    return std::holds_alternative<ErrMsg>(res);
}

template <typename T>
const T getVal(const Result<T, ErrMsg> res) {
    assert(std::holds_alternative<T>(res));
    return std::get<T>(res);
}

template <typename T>
const ErrMsg getErr(const Result<T, ErrMsg> res) {
    assert(isErr(res));
    return std::get<ErrMsg>(res);
}

}  // namespace SVFIR

#endif // RESULT_H
