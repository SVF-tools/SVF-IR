#include <algorithm>
#include <functional>
#include <unordered_set>

#include "SVFIR/Ast.h"
#include "SVFIR/Contracts.h"
#include "SVFIR/DominatorTree.h"

namespace {

using namespace SVFIR;
using Node = DominatorTree::Node;

static const size_t NO_DOM = size_t(-1);

/// This structure contains means to interpret the CFG appropriately for
/// calculating a dominator or post-dominator tree. This must be used when
/// accessing edges of a node and when accessing the entry node of the CFG.
/// For calculating dominator trees, it interprets the CFG as it is. For
/// calculating post-dominator trees, it
///   1) flips incoming and outgoing edges, and
///   2) treats the (unique) exit node as the entry node.
/// This avoids the need for either a separate implementation of the
/// construction algorithm or preparation/modification of the CFG beforehand.
struct CFGInterpreter {
    using Kind = DominatorTree::Kind;
    using CFGNode = ControlFlowGraph::Node;

    CFGInterpreter(const Kind kind)
    :
        inEdges(kind == Kind::DOM ? actualInEdges : actualOutEdges),
        outEdges(kind == Kind::DOM ? actualOutEdges : actualInEdges),
        entry(kind == Kind::DOM ? actualEntry : exit)
    { insist(kind == Kind::DOM || kind == Kind::POSTDOM); }

    /// "Incoming" edges of a node.
    std::function<const std::vector<Index> &(const CFGNode &)> inEdges;
    /// "Outgoing" edges of a node.
    std::function<const std::vector<Index> &(const CFGNode &)> outEdges;
    /// Entry node to the CFG.
    const std::function<Index(const ControlFlowGraph &)> entry;

private:
    static const std::vector<Index> &actualInEdges(const CFGNode &n) {
        return n.inEdges;
    }
    static const std::vector<Index> &actualOutEdges(const CFGNode &n) {
        return n.outEdges;
    }

    static Index actualEntry(const ControlFlowGraph &cfg) {
        return cfg.entry();
    }
    static Index exit(const ControlFlowGraph &cfg) {
        std::optional<Index> exit = cfg.uniqueExit();
        // Only used (as entry) when we get POSTDOM, so must have a unique exit.
        insist(exit.has_value());
        return exit.value();
    }
};

/// Return the postorder traversal order of the nodes in cfg starting from
/// the entry node (the first node). Nodes unreachable from the entry node
/// are excluded.
std::vector<Index> postOrder(
    const ControlFlowGraph &cfg,
    const CFGInterpreter &ci
) {
    const std::vector<ControlFlowGraph::Node> &nodes = cfg.nodes;
    insist(!nodes.empty());

    std::vector<Index> order;
    std::unordered_set<Index> visited;
    std::function<void(const Index)>
    visit = [&ci, &nodes, &visited, &order, &visit](const Index &cur) {
        if (visited.find(cur) == visited.cend()) {
            visited.insert(cur);
            for (const Index child : ci.outEdges(nodes[cur])) {
                visit(child);
            }
            order.push_back(cur);
        }
    };

    visit(ci.entry(cfg));

    return order;
}

/// Returns a vector mapping nodes to their immediate dominator node.
/// "A Simple, Fast Dominance Algorithm" -- Cooper, Harvey, Kennedy (p. 7).
/// TODO: this function is a mess. Needs a rewrite.
std::vector<Index> calculateIDoms(
    const ControlFlowGraph &cfg,
    const CFGInterpreter &ci
) {
    insist(!cfg.nodes.empty());

    // We do *everything* with the nodes labelled with the post-order traversal.
    // When dealing with post-order numbers, we are only dealing with reachable
    // nodes.
    const std::vector<Index> &po2id = postOrder(cfg, ci);
    // This is not a vector because unreachable nodes are not represented in po.
    std::unordered_map<Index, Index> id2po;
    for (size_t i = 0; i < po2id.size(); ++i) { id2po[po2id[i]] = i; }
    insist(id2po.size() == po2id.size());

    // Indices are POs too.
    std::vector<Index> podoms(po2id.size(), NO_DOM);
    const Index poentry = id2po[ci.entry(cfg)];
    podoms[poentry] = poentry;

    const auto &intersect = [&podoms](const Index b1, const Index b2) {
        Index finger1 = b1;
        Index finger2 = b2;
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
        for (Index b = (po2id.size() - 1) - 1; b != Index(-1); --b) {
            const ControlFlowGraph::Node &bn = cfg.nodes[po2id[b]];
            const std::vector<Index> &preds = ci.inEdges(bn);

            // Pick a processed/calculated predecessor.
            Index newIDom = NO_DOM;
            Index pickedPred = NO_DOM;
            for (const Index pred : preds) {
                const Index popred = id2po[pred];
                if (podoms[popred] != NO_DOM) {
                    pickedPred = pred;
                    newIDom = popred;
                    break;
                }
            }
            // b must have incoming edge because it is reachable (in po2id).
            insist(newIDom != NO_DOM && pickedPred != NO_DOM);

            for (const Index pred : preds) {
                // We are looking at all predecessors except the one we picked.
                if (pickedPred != pred) {
                    const Index p = id2po[pred];
                    if (podoms[p] != NO_DOM) { newIDom = intersect(p, newIDom); }
                }
            }

            if (podoms[b] != newIDom) {
                podoms[b] = newIDom;
                changed = true;
            }
        }
    }

    // Back from the post-order labelling to the real labels.
    // The unreachable nodes have NO_DOM.
    std::vector<Index> doms(cfg.nodes.size(), NO_DOM);
    for (Index i = 0; i < podoms.size(); ++i) {
        doms[po2id[i]] = po2id[podoms[i]];
    }

    // Nothing dominates the entry node.
    doms[ci.entry(cfg)] = NO_DOM;

    return doms;
}

/// Construct cfg's dominator tree, returns the root.
DominatorTree::Node build(
    const ControlFlowGraph &cfg,
    const CFGInterpreter &ci
) {
    insist(!cfg.nodes.empty());

    const std::vector<Index> &idoms = calculateIDoms(cfg, ci);
    std::vector<std::vector<Index>> childrenIndices(idoms.size());
    for (Index i = 0; i < idoms.size(); ++i) {
        if (idoms[i] != NO_DOM) {
            // Read as: idoms[i] dominates i, so idoms[i] is parent to i.
            childrenIndices[idoms[i]].push_back(i);
        }
    }

    std::function<Node(const Index)>
    mkNode = [&cfg, &childrenIndices, &mkNode](const Index &ni) {
        std::vector<Node> children;
        // This loop does not fire for leaf nodes, hence an empty children.
        for (const Index ci : childrenIndices[ni]) {
            children.push_back(mkNode(ci));
        }
        return Node(ni, cfg.nodes[ni].name, children);
    };

    return mkNode(ci.entry(cfg));
}

/// Returns any node that cannot be reached from cfg's entry node.
std::vector<Node> unreachableNodes(
    const ControlFlowGraph &cfg,
    const CFGInterpreter &ci
) {
    insist(!cfg.nodes.empty());

    std::vector<Node> unreachables;

    // Any "gaps" in the list of reachable nodes are unreachable nodes.
    std::vector<Index> reachables = postOrder(cfg, ci);
    std::sort(reachables.begin(), reachables.end());
    // To avoid special casing the end, we (potentially) induce a gap.
    // There is no need to special case at the start because the entry node is
    // 1) always there, and 2) always 0.
    reachables.push_back(cfg.nodes.size());

    // Minimum of size = 2, the entry node and the artifical size "node".
    for (Index i = 0; i < reachables.size() - 1; ++i) {
        const Index x = reachables[i];
        const Index y = reachables[i + 1];
        // Nodes in interval (x, y) are unreachable (may be empty).
        for (Index g = x + 1; g < y; ++g) {
            const Node &gn = Node(g, cfg.nodes[g].name, std::vector<Node>());
            unreachables.push_back(gn);
        }
    }

    return unreachables;
}

unsigned int countNodes(const DominatorTree::Node &root) {
    // 1 for root itself.
    unsigned int sum = 1;
    for (const Node &c : root.children) { sum += countNodes(c); }
    return sum;
}

}  // Anonymous namespace

namespace SVFIR {

DominatorTree::DominatorTree(const ControlFlowGraph &cfg, const Kind kind)
:
    kind(kind),
    root(build(cfg, CFGInterpreter(kind))),
    unreachables(unreachableNodes(cfg, CFGInterpreter(kind))),
    tour(mkTour())
{
    // It'll be checked in helpers, but it's nice to document.
    insist(!cfg.nodes.empty());
    insist(kind != Kind::POSTDOM || cfg.uniqueExit().has_value());
}

bool DominatorTree::dominates(const Index x, const Index y) const {
    insist(x < tour.arrival.size() && y < tour.arrival.size());

    for (const Node &u : unreachables) {
        if (u.id == x || u.id == y) { return false; }
    }

    return
        tour.arrival[x] < tour.arrival[y] &&
        tour.withdrawal[x] > tour.withdrawal[y];
}

DominatorTree::Tour DominatorTree::mkTour(void) const {
    using Timestamp = Tour::Timestamp;
    const unsigned int nodes = countNodes(root) + unreachables.size();
    std::vector<Timestamp> arrival(nodes, -1);
    std::vector<Timestamp> withdrawal(nodes, -1);

    // Start at -1, bump to arrive, bump to withdraw, return withdrawal time.
    std::function<Timestamp(const Node &, Timestamp)> visit =
    [&arrival, &withdrawal, &visit](const Node &n, Timestamp ts) {
        ++ts; arrival[n.id] = ts;
        for (const Node &c : n.children) { ts = visit(c, ts); }
        ++ts; withdrawal[n.id] = ts;
        return ts;
    };

    visit(root, -1);

    return Tour(arrival, withdrawal);
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

