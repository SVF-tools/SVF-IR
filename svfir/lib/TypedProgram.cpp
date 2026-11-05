#include "Ast.h"
#include "TypedProgram.h"

namespace SVFIR {

FnSig::FnSig(const Function &fn)
: params(fn.paramTypes()), ret(fn.type), vararg(fn.vararg)
{ }

bool FnSig::matches(const std::vector<Type> &args, const Type &exp) const {
    if (exp != ret) { return false; }
    else if (args.size() < params.size()) { return false; }
    else if (!vararg && args.size() != params.size()) { return false; }
    else {
        // The remaining cases are
        //   1. args and params have the same size (regardless of varargs),
        //   2. args > params and varargs
        // In both instances, we only need to concern ourselves with the
        // non-varargs part.
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i] != args[i]) { return false; }
        }

        return true;
    }
    unreachable();
}

}  // namespace SVFIR
