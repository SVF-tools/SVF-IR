#ifndef DOMINATOR_H
#define DOMINATOR_H

#include <string>
#include <vector>
#include <set>

#include "Ast.h"
#include "ControlFlowGraph.h"

namespace SVFIR {

/// Dominator tree, beginning at root, constructed from a CFG. In this tree,
/// a parent node is the immediate dominator of any children nodes. Being a
/// tree, thus, the root/entry dominates all other nodes in the tree
/// transitively. The root has no dominator. Nodes not reachable in the CFG
/// are not represented in the tree and are thus not deemed dominated by the
/// root (or by any node, including other unreachable nodes that may jump to
/// such nodes). Such nodes are saved in a separate field for reference.
///
/// This class can represent post-dominator trees too and the constructor
/// takes a switch on that. In that case the provided CFG must have a unique
/// exit (see unifyReturns). Everything about the previous paragraph holds
/// true for the post-dominator tree except:
/// 1) replace the term 'dominate' with 'post-dominate', and
/// 2) the root/entry is the unique exit of the CFG the tree is built from.
class DominatorTree {
public:
    enum class Kind { DOM, POSTDOM };

    struct Node {
        const Index id;
        // Not a key; only used for pretty printing, debugging, etc.
        const Name name;
        const std::vector<Node> children;

        Node(
            const Index id,
            const Name &name,
            const std::vector<Node> &children
        ) : id(id), name(name), children(children) { }

        /// [id name]
        std::string toAbbrString(void) const;
        /// [id name]
        /// children: ...
        std::string toString(void) const;

        // TODO: should children be a key at all?
        bool operator==(const Node &) const;
        bool operator<(const Node &) const;
    };

    /// On an Euler tour of the tree, entry and exit timestamps for each node
    /// (index). Every node has a timestamp. Timestamps increment upon visiting
    /// or leaving a node. The timestamps between arrival and withdrawal are
    /// mixed/continuous (thus arrival[x] < withdrawal[x] always). A negative
    /// timestamp indicates unreachability.
    struct Tour {
        using Timestamp = int;
        const std::vector<Timestamp> arrival;
        const std::vector<Timestamp> withdrawal;
        Tour(
            const std::vector<Timestamp> &arrival,
            const std::vector<Timestamp> &withdrawal
        ) : arrival(arrival), withdrawal(withdrawal) { }
    };

public:
    // Dominator tree vs post-dominator tree.
    const Kind kind;

    /// The root node, which corresponds to the entry of the CFG it was
    /// constructed from.
    const Node root;

    /// Nodes which are not reachable in the CFG this tree was constructed
    /// from (and are thus not dominated by root so are not part of the tree).
    const std::set<Node> unreachables;

    /// Order upon tree, for dominance queries.
    const Tour tour;

public:
    /// The control flow graph must be non-empty, i.e., must have an entry node.
    /// (Any CFG constructed from a valid function body fulfils this.)
    DominatorTree(const ControlFlowGraph &, const Kind kind);

    /// Whether x dominates y, appropriate to kind.
    bool dominates(const Index x, const Index y) const;

    std::string toString(void) const;
};

}  // namespace SVFIR

#endif // DOMINATOR_H
