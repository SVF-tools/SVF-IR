#include <unordered_map>
#include <vector>

#include "Contracts.h"
#include "Ast.h"
#include "ControlFlowGraph.h"

namespace {

using namespace SVFIR;

/// Return the outgoing targets, if any, of a basic block
std::vector<Id> getTargets(const BasicBlock &bb) {
    insist(!bb.stmts.empty());
    std::vector<Id> targets;
    const Statement &lastStmt = bb.stmts[bb.stmts.size() - 1];
    if (std::holds_alternative<BrStmt>(lastStmt)) {
        targets.push_back(std::get<BrStmt>(lastStmt).target);
    } else if (std::holds_alternative<BrifStmt>(lastStmt)) {
        const BrifStmt brif = std::get<BrifStmt>(lastStmt);
        targets.push_back(brif.ifTarget);
        targets.push_back(brif.elseTarget);
    } else {
        insist(std::holds_alternative<RetStmt>(lastStmt));
        // Does not target another block in the function.
    }

    return targets;
}

}  // Anonymous namespace

namespace SVFIR {

std::string ControlFlowGraph::Node::toString(void) const {
    std::string s = toAbbrString() + "\n";

    s += "in :";
    for (const NodeIndex &n : inEdges) { s += " " + std::to_string(n) + ","; }
    s.pop_back();  // The extra ",".
    s += "\n";

    s += "out:";
    for (const NodeIndex &n : outEdges) { s += " " + std::to_string(n) + ","; }
    s.pop_back();  // The extra ",".

    return s;
}

std::string ControlFlowGraph::Node::toAbbrString(void) const {
    return "[" + std::to_string(id) + " " + name + "]";
}

bool ControlFlowGraph::Node::isExitNode(void) const {
    return outEdges.empty();
}

ControlFlowGraph::ControlFlowGraph(const FunctionBody &body) {
    buildGraph(body);
}

void ControlFlowGraph::buildGraph(const FunctionBody &body) {
    std::unordered_map<Name, NodeIndex> names;
    for (size_t i = 0; i < body.size(); ++i) { names[body[i].id.name] = i; }

    // All nodes which have an outgoing edge to the key (the dst).
    std::unordered_map<NodeIndex, std::vector<NodeIndex>> sources;
    for (size_t dst = 0; dst < body.size(); ++dst) {
        const Name dstName = body[dst].id.name;
        for (size_t src = 0; src < body.size(); ++src) {
            for (const Id &id : getTargets(body[src])) {
                if (id.name == dstName) { sources[dst].push_back(src); }
            }
        }
    }

    for (size_t i = 0; i < body.size(); ++i) {
        const std::vector<NodeIndex> &inEdges = sources[i];
        std::vector<NodeIndex> outEdges;
        const std::vector<Id> &targets = getTargets(body[i]);
        for (const Id &tid : targets) {
            insist(names.count(tid.name) == 1);
            outEdges.push_back(names[tid.name]);
        }

        _nodes.push_back(Node(i, body[i].id.name, inEdges, outEdges));
    }
}

std::string ControlFlowGraph::toString(void) const {
    std::string s;
    for (const Node &node : _nodes) {
        s += node.toAbbrString() + "\n";

        // Just outgoing edges. Incoming edges will be dealt with at the source.
        if (node.outEdges.empty()) { s += "  --\n"; }
        else {
            for (const NodeIndex &dst : node.outEdges) {
                s += "  -> " + _nodes[dst].toAbbrString() + "\n";
            }
        }
    }

    // Drop the final newline.
    s.pop_back();

    return s;
}

}  // namespace SVFIR
