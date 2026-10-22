#ifndef CONTROLFLOWFGRAPH_H
#define CONTROLFLOWFGRAPH_H

#include "Ast.h"

namespace SVFIR {

/// Intra-procedural control flow graph. A CFG is an ordered vector of nodes.
/// The first node is the unique entry node. Node IDs correspond to their index
/// in said vector which correspond to the position of the basic block in the
/// function they are constructed from (i.e., the 0th (first) node has ID 0 and
/// correspond to the 0th (first) basic block, repeat for n).
/// That a node n has an incoming edge from node m means node m has an outgoing
/// edge to node n. A node with no outgoing edges is an exit node.
class ControlFlowGraph {
public:
    using NodeIndex = size_t;

    struct Node {
        const NodeIndex id;
        // Not a key; only used for pretty printing, debugging, etc.
        const Name name;
        const std::vector<NodeIndex> inEdges;
        const std::vector<NodeIndex> outEdges;

        Node(
            const NodeIndex &id,
            const Name &name,
            const std::vector<NodeIndex> &inEdges,
            const std::vector<NodeIndex> &outEdges
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

public:
    ControlFlowGraph(const FunctionBody &);

    const std::vector<Node> &nodes(void) const { return _nodes; }

    std::string toString(void) const;

private:
    /// As the name suggests. Sets the nodes field. To be called by constructor.
    void buildGraph(const FunctionBody &);

private:
    /// Described in class comment.
    std::vector<Node> _nodes;
};

}  // namespace SVFIR

#endif  // CONTROLFLOWGRAPH_H
