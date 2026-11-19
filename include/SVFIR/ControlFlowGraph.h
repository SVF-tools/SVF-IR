#ifndef CONTROLFLOWFGRAPH_H
#define CONTROLFLOWFGRAPH_H

#include <optional>
#include <vector>

#include "SVFIR/Ast.h"
#include "SVFIR/Index.h"

namespace SVFIR {

/// Intra-procedural control flow graph. A CFG is an ordered vector of nodes.
/// Node IDs correspond to their index in this vector which correspond to the
/// position of the basic block in the function they are constructed from (i.e.,
/// the 0th (first) node has ID 0 and correspond to the 0th (first) basic block,
/// repeat for n). That a node n has an incoming edge from node m means node m
/// has an outgoing edge to node n. A node with no outgoing edges is an exit
/// node. The first node is the unique entry node. A CFG can only be constructed
/// from a non-empty function body. Thus there is no CFG for an opaque function.
struct ControlFlowGraph {
    struct Node;

    /// Described in class comment.
    std::vector<Node> nodes;

    /// Note specifically that inEdges/outEdges are Indexes, not Nodes.
    /// They can be accessed through the containing CFG's nodes. Unideal...
    /// but it avoids the need for pointers.
    struct Node {
        const Index id;
        // Not a key; only used for pretty printing, debugging, etc.
        const Name name;
        const std::vector<Index> inEdges;
        const std::vector<Index> outEdges;

        Node(
            const Index &id,
            const Name &name,
            const std::vector<Index> &inEdges,
            const std::vector<Index> &outEdges
        ) : id(id), name(name), inEdges(inEdges), outEdges(outEdges)  { }

        /// Does this node only lead out of the function?
        bool isExitNode(void) const;

        /// [id name]
        /// in : ...
        /// out: ...
        std::string toString(void) const;
        /// [id name]
        std::string toAbbrString(void) const;
    };

    /// The given FunctionBody must be non-empty. (Aside, an empty body is not
    /// part of a valid program.)
    ControlFlowGraph(const FunctionBody &);

    /// The unique entry node.
    Index entry(void) const;

    /// Return this CFG's only exit node, if it has an only exit node.
    std::optional<Index> uniqueExit(void) const;

    /// The i-th node, i.e., nodes[i].
    const Node &node(const Index i) const;

    std::string toString(void) const;
};

}  // namespace SVFIR

#endif  // CONTROLFLOWGRAPH_H
