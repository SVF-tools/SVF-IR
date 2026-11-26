#include <optional>

#include "SVFIR/ControlFlowGraph.h"

#include "test.h"

using namespace SVFIR;

// We do not need operator== on ControlFlowGraph::Nodes except for testing,
// thus we implement it here, not in ControlflowGraph.
namespace SVFIR {

bool operator==(
    const ControlFlowGraph::Node &x, const ControlFlowGraph::Node &y
) {
    return
        x.id == y.id &&
        x.name == y.name &&
        x.inEdges == y.inEdges &&
        x.outEdges == y.outEdges;
}

}  // namespace SVFIR

bool testCfg1(void) {
    using Node = ControlFlowGraph::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!0 (
              (alloc heap %y)
              (br !1)))
            (!1 ((br !2)))
            (!2 (
              (cmp = %c %x (i32 1))
              (brif %c !1 !3)))
            (!3 ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());

    const Node &x = Node(0, "!0", {}, {1});
    const Node &y = Node(0, "!0", {}, {1});

    static const std::vector<Node> &ens =  {
        Node(0, "!0", {}, {1}),
        Node(1, "!1", {0, 2}, {2}),
        Node(2, "!2", {1}, {1, 3}),
        Node(3, "!3", {2}, {})
    };
    TEST(ens == ens);
    TEST(cfg.entry() == 0);
    TEST(cfg.uniqueExit().has_value());
    TEST(cfg.uniqueExit().value() == 3);

    return true;
}
