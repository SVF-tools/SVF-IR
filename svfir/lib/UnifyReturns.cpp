#include <unordered_set>

#include "Ast.h"
#include "Ops.h"
#include "Span.h"
#include "Statements.h"
#include "UnifyReturns.h"
#include "TypedProgram.h"

namespace {

using namespace SVFIR;

/// The number of return instructions scattered in fn. Opaque functions have
/// 0 returns.
unsigned int countReturns(const Function &fn) {
    if (fn.isOpaque()) { return 0; }
    unsigned int rets = 0;
    for (const BasicBlock &bb : fn.body.value()) {
        if (isRet(bb.stmts.back())) { ++rets; }
    }
    return rets;
}

/// Return an Id, with a name beginning with prefix (after a prepended '!'),
/// suitable to be used as a local ID and unique to any other blocks in fn.
Id uniqueBlockId(const Function &fn, const std::string prefix) {
    insist(!empty(prefix));
    std::string newName = '!' + prefix;
    if (!fn.isOpaque()) {
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

    std::unordered_set<Name> names;
    for (const Param &p : fn.params) { names.insert(p.id.name); }
    if (!fn.isOpaque()) {
        for (const BasicBlock &bb : fn.body.value()) {
            for (const Statement &s : bb.stmts) {
                const std::optional<Id> id = introducedVar(s);
                if (id.has_value()) { names.insert(id.value().name); }
            }
        }
    }
    while (names.find(newName) != names.cend()) { newName += "_"; }

    return Id(newName, FAB_SPAN);
}

/// Returns an equivalent to fn with all returns unified. THat means
/// potentially 1) a new block with a single return, 2) a phi in that
/// block to gather all return values, and 3) jumps to that block in
/// place of returns.
Function unify(const Function &fn) {
    // TODO: should add metadata stating the new stuff is from this.

    // Nothing to unify.
    if (fn.isOpaque()) { return fn; }
    const unsigned int nrets = countReturns(fn);
    if (nrets == 0 || nrets == 1) { return fn; }

    // Blocks ending in ret will now target urid. urid will be the final block.
    const Id urid = uniqueBlockId(fn, "unified-ret");
    std::vector<BasicBlock> newBlocks;
    std::vector<PhiStmt::Arg> phiArgs;

    // Modifications to the existing blocks.
    for (const BasicBlock &bb : fn.body.value()) {
        if (!isRet(bb.stmts.back())) { newBlocks.push_back(bb); }
        else {
            const RetStmt &ret = std::get<RetStmt>(bb.stmts.back());
            // No phi when returning void.
            if (ret.val.has_value()) {
                const Span sp = Span(
                    span(ret.val.value()).start,
                    span(ret.val.value()).end,
                    true
                );
                phiArgs.push_back(PhiStmt::Arg(ret.val.value(), bb.id, sp));
            }
            // Unconditional jump to the unified return as terminating statement.
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
        const Id pid = uniqueLocalId(fn, "unified-ret");
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

Program unifyReturns(const TypedProgram &tp) {
    const Program &p = tp.program();
    std::vector<Function> unifiedFunctions;
    for (const Function &fn : p.functions) {
        if (fn.isOpaque()) { unifiedFunctions.push_back(fn); }
        else {
            const FunctionBody &bbs = fn.body.value();
            unifiedFunctions.push_back(unify(fn));
        }
    }

    return Program(
        p.preamble, p.types, p.variables, unifiedFunctions, p.md, p.span
    );
}

}  // namespace SVFIR
