#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SVFIR/Ast.h"
#include "SVFIR/AstHelpers.h"
#include "SVFIR/Err.h"
#include "SVFIR/Index.h"
#include "SVFIR/Linker.h"
#include "SVFIR/TypedProgram.h"
#include "SVFIR/Variant.h"

namespace {

using namespace SVFIR;

/// Shorthand to gather functions/variables from both the left and right
/// programs. Since we are to reproduce the left, we want all
/// variables/functions from the left and we are only interested in the
/// right when it has something (and non-opaque) corresponding on the left.
/// It is up to the caller to track whether the index is into the list
/// of functions or variables.
struct Symbol {
    /// Index of the variable/function in the left program.
    const Index left;
    /// Index of the variable/function in the right program, if it exists.
    const std::optional<Index> right;

    Symbol(const Index left, const std::optional<Index> &right)
    : left(left), right(right) { }
};

/// Returns an error if any names are shared in leftKinds and rightNotKinds.
template <typename Kind, typename NotKind>
std::optional<Err> checkKindsMatch(
    const std::vector<Kind> &leftKinds,
    const std::vector<NotKind> &rightNotKinds,
    const std::string &kindDesc,
    const std::string &notKindDesc
) {
    std::unordered_set<Name> leftNames;
    for (const Kind &k : leftKinds) { leftNames.insert(k.id.name); }

    for (const NotKind &nk : rightNotKinds) {
        if (leftNames.find(nk.id.name) != leftNames.cend()) {
            return Err(
                "'" + nk.id.name + "' is a " + kindDesc + " on the left "
                "and a " + notKindDesc + " on the right.",
                span(nk)
            );
        }
    }

    return std::nullopt;
}

/// Checks that symbols all have a matching signature in the left and right, if
/// applicable.
std::optional<Err> checkSigsMatch(
    const std::vector<Symbol> &fnSymbols,
    const std::vector<Function> &leftFunctions,
    const std::vector<Function> &rightFunctions
) {
    for (const Symbol &sym : fnSymbols) {
        if (sym.right.has_value()) {
            const Function &rfn = rightFunctions[sym.right.value()];
            const FnSig &lsig = FnSig(leftFunctions[sym.left]);
            const FnSig &rsig = FnSig(rfn);
            if (lsig != rsig) {
                return Err(
                    "The function '" + rfn.id.name + "' appears on both the "
                    "left and the right with different signatures",
                    span(rfn)
                );
            }
        }
    }

    return std::nullopt;
}

/// Return a list placing the symbols of the same name on the left and right.
/// Names on the left without a corresponding right symbol are included.
/// Names on the right without a corresponding left symbol are excluded.
template <typename Kind>
std::vector<Symbol> mergeIntoSymbols(
    const std::vector<Kind> left,
    const std::vector<Kind> right
) {
    std::unordered_map<Name, Index> rightIndices;
    for (Index i = 0; i < right.size(); ++i) {
        rightIndices.insert({right[i].id.name, i});
    }

    std::vector<Symbol> symbols;
    for (Index li = 0; li < left.size(); ++li) {
        const auto &rpos = rightIndices.find(left[li].id.name);
        if (rpos != rightIndices.cend()) {
            symbols.emplace_back(li, rpos->second);
        } else { symbols.emplace_back(li, std::nullopt); }
    }

    return symbols;
}

}  // Anonymous namespace

namespace SVFIR {

// TODO: metadata is not handled!
// TODO: account for version compatibility.
Result<TypedProgram> link(
    const TypedProgram &left,
    const TypedProgram &right,
    const Linkage linkage
) {
    const std::vector<Function> &lfns = left.program.functions;
    const std::vector<Function> &rfns = right.program.functions;
    const std::vector<GlobalVar> &lgvs = left.program.globalVars;
    const std::vector<GlobalVar> &rgvs = right.program.globalVars;

    const std::optional<Err> &e1 = checkKindsMatch(
        lfns, rgvs, "function", "global variable"
    );
    if (e1.has_value()) { return e1.value(); }

    const std::optional<Err> &e2 = checkKindsMatch(
        lgvs, rfns, "global variable", "function"
    );
    if (e2.has_value()) { return e2.value(); }

    const std::vector<Symbol> &fnSymbols = mergeIntoSymbols(lfns, rfns);

    const std::optional<Err> &e3 = checkSigsMatch(fnSymbols, lfns, rfns);
    if (e3.has_value()) { return e3.value(); }

    std::vector<Function> linkedFns;
    for (const Symbol &sym : fnSymbols) {
        if (!sym.right.has_value()) {
            // Left has or has not a body, right does not exist.
            linkedFns.push_back(lfns[sym.left]);
        } else {
            const Function &lfn = lfns[sym.left];
            const Function &rfn = rfns[sym.right.value()];
            if (!lfn.isOpaque() && !rfn.isOpaque()) {
                // Left has a body, right does too.
                if (linkage == Linkage::STRONG) {
                    return Err(
                        "Function '" + lfn.id.name + "' has a body on both "
                        "the left and right in the presence of strong linkage.",
                        span(rfn)
                    );
                } else {
                    insist(linkage == Linkage::WEAK);
                    linkedFns.push_back(lfn);
                }
            } else {
                if (!rfn.isOpaque()) {
                    // Left has no body, right does.
                    linkedFns.emplace_back(
                        lfn.id,
                        lfn.params,
                        lfn.vararg,
                        lfn.type,
                        rfn.body,
                        // TODO: merge metadata.
                        lfn.md,
                        // Give it left's span. The body will have spans
                        // specific to it.
                        lfn.span
                    );
                } else {
                    // Left has no body, right does not too.
                    linkedFns.push_back(lfn);
                }
            }
        }
    }

    const std::vector<Symbol> &gvSymbols = mergeIntoSymbols(lgvs, rgvs);
    std::vector<GlobalVar> linkedGvs;
    // TODO: pull this into separate function.
    for (const Symbol &sym : gvSymbols) {
        if (!sym.right.has_value()) {
            // Left has or has not a value, right does not.
            linkedGvs.push_back(lgvs[sym.left]);
        } else {
            const GlobalVar &lgv = lgvs[sym.left];
            const GlobalVar &rgv = rgvs[sym.right.value()];
            if (!lgv.isOpaque() && !rgv.isOpaque()) {
                // Left has a body, right does too.
                if (linkage == Linkage::STRONG) {
                    return Err(
                        "Global variable '" + lgv.id.name + "' is initialised "
                        "on both the left and right in the presence of strong "
                        "linkage.",
                        span(rgv)
                    );
                } else {
                    insist(linkage == Linkage::WEAK);
                    linkedGvs.push_back(lgv);
                }
            } else {
                if (!rgv.isOpaque()) {
                    // Left has no body, right does.
                    // Keep left span, initialiser will have its own.
                    // TODO: merge metadata.
                    linkedGvs.emplace_back(lgv.id, rgv.val, lgv.md, lgv.span);
                } else {
                    // Left has no body, right does not too.
                    linkedGvs.push_back(rgv);
                }
            }
        }
    }

    const Result<TypedProgram> &newProgram = typeProgram(Program(
        Preamble(left.program.preamble.version, "linker", FAB_SPAN),
        linkedGvs,
        linkedFns,
        left.program.md,
        left.program.span
    ));
    insist(!is<Err>(newProgram));
    return get<TypedProgram>(newProgram);
}

}  // namespace SVFIR
