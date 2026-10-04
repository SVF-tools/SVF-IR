#include <iostream>

/// When expr evaluates to false prints an error message and returns false from
/// enclosing function.
#define TEST(expr)\
    if (!(expr)) {\
        std::cerr\
            << "FAILURE at " << __FILE__ << ":" << __LINE__\
            << " '" << #expr << "'"\
            << std::endl;\
        return false;\
    }
