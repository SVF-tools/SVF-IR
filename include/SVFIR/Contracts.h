#include <cstdlib>
#include <iostream>

/// When expr evaluates to false prints an error message and aborts.
#define insist(expr)\
    if (!(expr)) {\
        std::cerr\
            << "Failed at " << __FILE__ << ":" << __LINE__ << ". "\
            << "Insisted '" << #expr << "'"\
            << std::endl;\
        std::abort();\
    }

/// Aborts when reached.
#define unreachable()\
    std::cerr\
        << "Reached unreachable at "\
        << __FILE__ << ":" << __LINE__\
        << std::endl;\
    std::abort();
