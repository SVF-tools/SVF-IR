#include <unordered_set>
#include <algorithm>
#include "timequick.h"

#include "Analyses.h"
#include "Ast.h"
#include "Index.h"
#include "Ops.h"
#include "Span.h"
#include "Statements.h"
#include "Transformations.h"
#include "TypedProgram.h"
#include "Variant.h"

namespace {

using namespace SVFIR;

/// The number of return instructions scattered in fn. Opaque functions have
/// 0 returns.
unsigned int countReturns(const Function &fn) {
    if (fn.isOpaque()) { return 0; }
    unsigned int rets = 0;
    for (const BasicBlock &bb : fn.body.value()) {
        if (is<RetStmt>(bb.stmts.back())) { ++rets; }
    }
    return rets;
}

/// Return an Id, with a name beginning with prefix (after a prepended '!'),
/// suitable to be used as a local ID and unique to any other blocks in fn.
Id uniqueBlockId(const Function &fn, const std::string prefix) {
    insist(!empty(prefix));
    std::string newName = '!' + prefix;
    if (!fn.isOpaque()) {
        /// Function is called only once so doing this is fine.
        std::unordered_set<Name> names;
        for (const BasicBlock &bb : fn.body.value()) {
            names.insert(bb.id.name);
        }
        while (names.find(newName) != names.cend()) { newName += "_"; }
    }
    return Id(newName, FAB_SPAN);
}

/// Return an Id, with a name beginning with prefix (after a prepended '%'),
/// suitable to be used as a local ID and unique to any other blocks in fn.
Id uniqueLocalId(const Function &fn, const std::string prefix) {
    insist(!empty(prefix));
    std::string newName = '%' + prefix;

    /// Function is called only once so doing this is fine.
    std::unordered_set<Name> names;
    for (const Param &p : fn.params) { names.insert(p.id.name); }
    if (!fn.isOpaque()) {
        for (const BasicBlock &bb : fn.body.value()) {
            for (const Statement &s : bb.stmts) {
                const std::optional<Id> &id = introducedVar(s);
                if (id.has_value()) { names.insert(id.value().name); }
            }
        }
    }
    while (names.find(newName) != names.cend()) { newName += "_"; }

    return Id(newName, FAB_SPAN);
}

/// Return true when this function needs returns unified. If this is not
/// the case, fn has a body and more than 1 return.
bool needsUnification(const Function &fn) {
    if (fn.isOpaque()) { return false; }
    const unsigned int nrets = countReturns(fn);
    if (nrets == 0 || nrets == 1) { return false; }
    return true;
}

/// Returns an equivalent to fn with all returns unified. THat means
/// potentially 1) a new block with a single return, 2) a phi in that
/// block to gather all return values, and 3) jumps to that block in
/// place of returns.
/// fn *must* be in need of unification (needsUnification).
Function unify(const Function &fn) {
    // TODO: should add metadata stating the new stuff is from this.

    if (!needsUnification(fn)) { return fn; };

    // Blocks ending in ret will now target urid. urid will be the final block.
    const Id &urid = uniqueBlockId(fn, "unified-ret");
    std::vector<BasicBlock> newBlocks;
    std::vector<PhiStmt::Arg> phiArgs;

    // Modifications to the existing blocks.
    for (const BasicBlock &bb : fn.body.value()) {
        if (!is<RetStmt>(bb.stmts.back())) { newBlocks.push_back(bb); }
        else {
            const RetStmt &ret = get<RetStmt>(bb.stmts.back());
            // Phi only when returning a value (i.e. not void).
            if (ret.val.has_value()) {
                const Span &sp = Span(
                    span(ret.val.value()).start,
                    span(ret.val.value()).end,
                    true
                );
                phiArgs.push_back(PhiStmt::Arg(ret.val.value(), bb.id, sp));
            }
            // Replace return with unconditional jump to unified return block.
            std::vector<Statement> newStmts(bb.stmts);
            newStmts.pop_back();
            newStmts.push_back(
                BrStmt(urid, ret.md, Span(ret.span.start, ret.span.end, true))
            );
            // TODO: maybe mark span as gen?
            newBlocks.push_back(
                BasicBlock(bb.id, newStmts, std::nullopt, bb.span)
            );
        }
    }

    // The unified return block.
    std::vector<Statement> urStmts;
    if (!phiArgs.empty()) {
        // Non-void.
        const Id &pid = uniqueLocalId(fn, "unified-ret");
        urStmts.push_back(PhiStmt(pid, phiArgs, std::nullopt, FAB_SPAN));
        urStmts.push_back(RetStmt(pid, std::nullopt, FAB_SPAN));
    } else {
        // Void.
        urStmts.push_back(RetStmt(std::nullopt, std::nullopt, FAB_SPAN));
    }
    newBlocks.push_back(BasicBlock(urid, urStmts, std::nullopt, FAB_SPAN));

    return Function(
        fn.id, fn.params, fn.vararg, fn.type, newBlocks, fn.md, fn.span
    );
}

}  // Anonymous namespace

namespace SVFIR {

TypedProgram unifyReturns(const TypedProgram &tp) {
    const Program &p = tp.program;

    if (
        std::all_of(
            p.functions.cbegin(),
            p.functions.cend(),
            [](const Function &fn) { return !needsUnification(fn); }
        )
    ) { return tp; }

    std::vector<Function> unifiedFunctions;
    for (const Function &fn : p.functions) {
        unifiedFunctions.push_back(unify(fn));
    }

    const Program &up =
        Program(p.preamble, p.globalVars, unifiedFunctions, p.md, p.span);

    // For any function where unification took place, we *may* have introduced
    // a single new identifier. It does not have a type and we will now give it
    // a type. We'll iterate over the original program to see which functions
    // were actually unified.
    // TODO: can avoid this if the only functions unified return void.
    SymbolTable::FnTables fnTables = tp.symbols.fnTables;
    for (Index i = 0; i < p.functions.size(); ++i) {
        if (needsUnification(p.functions[i])) {
            const Function &ufn = up.functions[i];
            if (!is<VoidType>(ufn.type)) {
                for (const BasicBlock &bb : ufn.body.value()) {
                    if (is<RetStmt>(bb.stmts.back())) {
                        // At this point we are in a function with returns
                        // unified by the transformation pass, which is
                        // non-void, and we are in the sole basic block
                        // with the return. The first statement of this
                        // block must be the introduced phi statement.
                        insist(is<PhiStmt>(bb.stmts.front()));
                        const Id &phiId = get<PhiStmt>(bb.stmts.front()).id;
                        insist(!fnTables.at(ufn.id.name).count(phiId.name));
                        // The introduced variable will be returned so its
                        // type must be the return type.
                        fnTables.at(ufn.id.name).insert({phiId.name, ufn.type});
                    }
                }
            }
        }
    }

    return TypedProgram(
        up,
        SymbolTable(tp.symbols.fnSigs, tp.symbols.globalVarTypes, fnTables)
    );
}

}  // namespace SVFIR
