#include <algorithm>
#include <functional>
#include <set>
#include <unordered_set>

#include "Ast.h"
#include "Contracts.h"
#include "Dominator.h"

namespace {

using namespace SVFIR;
using NodeIndex = DominatorTree::NodeIndex;
using Node = DominatorTree::Node;

static const size_t undef = size_t(-1);

/// Return the postorder traversal order of the nodes in cfg starting from
/// the entry node (the first node). Nodes unreachable from the entry node
/// are excluded.
std::vector<NodeIndex> postOrder(const ControlFlowGraph &cfg) {
    const std::vector<ControlFlowGraph::Node> nodes = cfg.nodes();
    insist(!nodes.empty());

    std::vector<NodeIndex> order;
    std::unordered_set<NodeIndex> visited;
    std::function<void(const NodeIndex)>
    visit = [&nodes, &visited, &order, &visit](const NodeIndex &cur) {
        if (visited.find(cur) == visited.cend()) {
            visited.insert(cur);
            for (const NodeIndex child : nodes[cur].outEdges) { visit(child); }
            order.push_back(cur);
        }
    };

    visit(nodes.front().id);
    return order;
}

/// Returns a vector mapping nodes to their immediate dominator node.
/// "A Simple, Fast Dominance Algorithm" -- Cooper, Harvey, Kennedy (p. 7).
std::vector<NodeIndex> calculateIDoms(const ControlFlowGraph &cfg) {
    static const size_t undef = size_t(-1);

    insist(!cfg.nodes().empty());

    // We do *everything* with the nodes labelled with the post-order traversal.
    const std::vector<NodeIndex> po2id = postOrder(cfg);
    // This is not a vector because unreachable nodes are not represented in po.
    std::unordered_map<NodeIndex, NodeIndex> id2po;
    for (size_t i = 0; i < po2id.size(); ++i) { id2po[po2id[i]] = i; }


    std::vector<NodeIndex> podoms(cfg.nodes().size(), undef);
    // The entry node is the last node in postorder.
    podoms.back() = podoms.size() - 1;

    const auto intersect = [&podoms](const NodeIndex b1, const NodeIndex b2) {
        NodeIndex finger1 = b1;
        NodeIndex finger2 = b2;
        while (finger1 != finger2) {
            while (finger1 < finger2) { finger1 = podoms[finger1]; }
            while (finger2 < finger1) { finger2 = podoms[finger2]; }
        }
        return finger1;
    };

    bool changed = true;
    while (changed) {
        changed = false;
        // -1-1 drops the entry node, which in a postorder must be the last.
        for (NodeIndex b = (po2id.size() - 1) - 1; b != NodeIndex(-1); --b) {
            const ControlFlowGraph::Node &bn = cfg.nodes()[po2id[b]];
            // b must have incoming edge because it is reachable (in po2id).
            NodeIndex newIDom = id2po[bn.inEdges.front()];
            for (
                auto it = bn.inEdges.cbegin() + 1; it != bn.inEdges.cend(); ++it
            ) {
                const NodeIndex p = id2po[*it];
                if (podoms[p] != undef) { newIDom = intersect(p, newIDom); }
            }

            if (podoms[b] != newIDom) {
                podoms[b] = newIDom;
                changed = true;
            }
        }
    }

    // Back from the post-order labelling to the real labels.
    std::vector<NodeIndex> doms(podoms.size(), undef);
    for (NodeIndex i = 0; i < podoms.size(); ++i) {
        doms[po2id[i]] = po2id[podoms[i]];
    }

    // Nothing dominates the entry node.
    doms[cfg.entry()] = undef;

    return doms;
}

/// Construct cfg's dominator tree, returns the root.
DominatorTree::Node build(const ControlFlowGraph &cfg) {
    insist(!cfg.nodes().empty());

    const std::vector<NodeIndex> idoms = calculateIDoms(cfg);
    std::vector<std::vector<NodeIndex>> childrenIndices(idoms.size());
    for (NodeIndex i = 0; i < idoms.size(); ++i) {
        if (idoms[i] != undef) {
            // Read as: idoms[i] dominates i, so idoms[i] is parent to i.
            childrenIndices[idoms[i]].push_back(i);
        }
    }

    std::function<Node(const NodeIndex)>
    mkNode = [&cfg, &childrenIndices, &mkNode](const NodeIndex &ni) {
        std::vector<Node> children;
        // This loop does not fire for leaf nodes, hence an empty children.
        for (const NodeIndex ci : childrenIndices[ni]) {
            children.push_back(mkNode(ci));
        }
        return Node(ni, cfg.nodes()[ni].name, children);
    };

    return mkNode(cfg.entry());
}

/// Returns any node that cannot be reached from cfg's entry node.
std::set<Node> unreachableNodes(const ControlFlowGraph &cfg) {
    insist(!cfg.nodes().empty());

    std::set<Node> unreachables;

    // Any "gaps" in the list of reachable nodes are unreachable nodes.
    std::vector<NodeIndex> reachables = postOrder(cfg);
    std::sort(reachables.begin(), reachables.end());
    // To avoid special casing the end, we (potentially) induce a gap.
    // There is no need to special case at the start because the entry node is
    // 1) always there, and 2) always 0.
    reachables.push_back(cfg.nodes().size());

    // Minimum of size = 2, the entry node and the artifical size "node".
    for (NodeIndex i = 0; i < reachables.size() - 1; ++i) {
        const NodeIndex x = reachables[i];
        const NodeIndex y = reachables[i + 1];
        // Nodes in interval (x, y) are unreachable (may be empty).
        for (NodeIndex g = x + 1; g < y; ++g) {
            const Node gn = Node(g, cfg.nodes()[g].name, std::vector<Node>());
            unreachables.insert(gn);
        }
    }

    return unreachables;
}

}  // Anonymous namespace

namespace SVFIR {

DominatorTree::DominatorTree(const ControlFlowGraph &cfg)
: root(build(cfg)), unreachables(unreachableNodes(cfg)) {
    // It'll be checked in helpers, but it's nice to document.
    insist(!cfg.nodes().empty());
}

std::string DominatorTree::toString(void) const {
    static std::function<std::string(const Node, const unsigned int)>
    treeNode = [](const Node &n, const unsigned int indentLevel) {
        std::string s;
        for (unsigned int i = 0; i < indentLevel; ++i) { s += "  "; }
        s += n.toAbbrString() + "\n";
        for (const Node &cn : n.children) { s += treeNode(cn, indentLevel + 1); }
        return s;
    };
    std::string s = treeNode(root, 0);
    // Includes a trailing \n.

    if (unreachables.empty()) {
        s += "(all nodes reachable.)";
    } else {
        s += "(unreachable nodes:";
        for (const Node &n : unreachables) { s += " " + n.toAbbrString(); }
    }

    return s;
}

bool DominatorTree::Node::operator==(const Node &n) const {
    // Ignore name, it is not a key.
    return this->id == n.id && this->children == n.children;
}

bool DominatorTree::Node::operator<(const Node &n) const {
    // Ignore name, it is not a key.
    return this->id < n.id || this->children < n.children;
}

std::string DominatorTree::Node::toString(void) const {
    std::string s = toAbbrString() + "\n";

    s += "children :";
    for (const Node &n : children) { s += " " + n.toAbbrString() + ","; }
    s.pop_back();  // The extra ",".

    return s;
}

std::string DominatorTree::Node::toAbbrString(void) const {
    return "[" + std::to_string(id) + " " + name + "]";
}

}  // namespace SVFIR

