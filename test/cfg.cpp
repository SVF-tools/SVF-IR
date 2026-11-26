#include <optional>

#include "SVFIR/ControlFlowGraph.h"

#include "test.h"

using namespace SVFIR;

// We do not need operator== on ControlFlowGraph::Nodes except for testing,
// thus we implement it here in test, not in the library.
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
    static const std::vector<Node> &ens =  {
        Node(0, "!0", {}, {1}),
        Node(1, "!1", {0, 2}, {2}),
        Node(2, "!2", {1}, {1, 3}),
        Node(3, "!3", {2}, {})
    };
    TEST(cfg.nodes == ens);
    TEST(cfg.entry() == 0);
    TEST(cfg.uniqueExit().has_value());
    TEST(cfg.uniqueExit().value() == 3);

    return true;
}

bool testCfg2(void) {
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
            (!3 ((ret void)))
            (!4 ((ret void)))
            (!5 ((br !1))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    static const std::vector<Node> &ens =  {
        Node(0, "!0", {}, {1}),
        Node(1, "!1", {0, 2, 5}, {2}),
        Node(2, "!2", {1}, {1, 3}),
        Node(3, "!3", {2}, {}),
        Node(4, "!4", {}, {}),
        Node(5, "!5", {}, {1})
    };
    TEST(cfg.nodes == ens);
    TEST(cfg.entry() == 0);
    TEST(!cfg.uniqueExit().has_value());

    return true;
}

bool testCfg3(void) {
    using Node = ControlFlowGraph::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!x ((br !y)))
            (!y ((br !y))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    static const std::vector<Node> &ens =  {
        Node(0, "!x", {}, {1}), Node(1, "!y", {0, 1}, {1})
    };
    TEST(cfg.nodes == ens);
    TEST(cfg.entry() == 0);
    TEST(!cfg.uniqueExit().has_value());

    return true;
}

bool testCfg4(void) {
    using Node = ControlFlowGraph::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f ptr ((i32 %x)) (
            (!x ((brif (i8 0) !y !z)))
            (!y ((brif (i8 0) !y !z)))
            (!z ((ret (ptr null)))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    static const std::vector<Node> &ens =  {
        Node(0, "!x", {}, {1, 2}),
        Node(1, "!y", {0, 1}, {1, 2}),
        Node(2, "!z", {0, 1}, {})
    };
    TEST(cfg.nodes == ens);
    TEST(cfg.entry() == 0);
    TEST(cfg.uniqueExit().has_value());
    TEST(cfg.uniqueExit().value() == 2);

    return true;
}
