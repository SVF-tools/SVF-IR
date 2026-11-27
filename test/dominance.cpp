#include <algorithm>
#include <optional>

#include "SVFIR/ControlFlowGraph.h"
#include "SVFIR/DominatorTree.h"
#include "SVFIR/Index.h"

#include "test.h"

using namespace SVFIR;

// TODO: really complex test once we have some bigger programs.

// We do not need operator== on DominatorTree::Nodes except for testing,
// thus we implement it here in test, not in the library.
namespace SVFIR {

bool operator==(const DominatorTree::Node &x, const DominatorTree::Node &y) {
    bool a = x.id == y.id && x.name == y.name && x.children == y.children;
    return a;
}

bool operator==(
    const std::vector<DominatorTree::Node> &xs,
    const std::vector<DominatorTree::Node> &ys
) {
    if (xs.size() != ys.size()) { return false; }
    else {
        for (size_t i = 0; i < xs.size(); ++i) {
            if (!(xs[i] == ys[i])) { return false; }
        }
        return true;
    }
    unreachable();
}

}  // namespace SVFIR

bool testDominance1(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!0 ((br !1)))
            (!1 ((br !2)))
            (!2 ((br !3)))
            (!3 ((br !1))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);
    static const Node &n3 = Node(3, "!3", std::vector<Node>());
    static const Node &n2 = Node(2, "!2", {n3});
    static const Node &n1 = Node(1, "!1", {n2});
    static const Node &et = Node(0, "!0", {n1});

    TEST(dt.root == et);
    TEST(dt.unreachables.empty());

    TEST(dt.dominates(0, 1));
    TEST(dt.dominates(0, 2));
    TEST(dt.dominates(0, 3));
    TEST(dt.dominates(1, 2));
    TEST(dt.dominates(1, 3));
    TEST(dt.dominates(2, 3));

    TEST(!dt.dominates(2, 2));
    TEST(!dt.dominates(1, 1));
    TEST(!dt.dominates(1, 0));
    TEST(!dt.dominates(3, 1));

    return true;
}

bool testDominance2(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!0 ((brif (i8 1) !1 !2)))
            (!1 ((ret void)))
            (!2 ((brif (i8 1) !3 !4)))
            (!3 ((brif (i8 1) !2 !4)))
            (!4 ((ret void))) )))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    static const Node &n4 = Node(4, "!4", std::vector<Node>());
    static const Node &n3 = Node(3, "!3", std::vector<Node>());
    static const Node &n2 = Node(2, "!2", {n3, n4});
    static const Node &n1 = Node(1, "!1", std::vector<Node>());
    static const Node &et = Node(0, "!0", {n1, n2});

    TEST(dt.root == et);
    TEST(dt.unreachables.empty());

    TEST(dt.dominates(0, 1));
    TEST(dt.dominates(0, 2));
    TEST(dt.dominates(0, 3));
    TEST(dt.dominates(0, 4));
    TEST(dt.dominates(2, 3));
    TEST(dt.dominates(2, 4));

    TEST(!dt.dominates(0, 0));
    TEST(!dt.dominates(1, 2));
    TEST(!dt.dominates(1, 3));
    TEST(!dt.dominates(1, 4));
    TEST(!dt.dominates(3, 4));
    TEST(!dt.dominates(4, 3));
    TEST(!dt.dominates(2, 1));
    TEST(!dt.dominates(4, 0));

    return true;
}

bool testDominance3(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!x ((ret void)))
            (!y ((ret void)))
            (!z ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    static const Node &x = Node(0, "!x", std::vector<Node>());
    static const Node &y = Node(1, "!y", std::vector<Node>());
    static const Node &z = Node(2, "!z", std::vector<Node>());

    TEST(dt.root == x);
    static const std::vector<Node> &eu = {y, z};
    TEST(dt.unreachables == eu);

    TEST(!dt.dominates(0, 0));
    TEST(!dt.dominates(0, 1));
    TEST(!dt.dominates(0, 2));
    TEST(!dt.dominates(1, 1));
    TEST(!dt.dominates(1, 2));
    TEST(!dt.dominates(2, 2));
    TEST(!dt.dominates(1, 0));
    TEST(!dt.dominates(2, 0));
    TEST(!dt.dominates(2, 1));

    return true;
}

bool testDominance4(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!w ((br !y)))
            (!x ((ret void)))
            (!y ((ret void)))
            (!z ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &dt = DominatorTree(cfg, DominatorTree::Kind::DOM);

    static const Node &z = Node(3, "!z", std::vector<Node>());
    static const Node &y = Node(2, "!y", std::vector<Node>());
    static const Node &x = Node(1, "!x", std::vector<Node>());
    static const Node &w = Node(0, "!w", {y});

    TEST(dt.root == w);
    static const std::vector<Node> &eu = {x, z};
    TEST(dt.unreachables == eu);

    TEST(dt.dominates(0, 2));

    TEST(!dt.dominates(0, 0));
    TEST(!dt.dominates(0, 1));
    TEST(!dt.dominates(0, 3));
    TEST(!dt.dominates(1, 1));
    TEST(!dt.dominates(1, 2));
    TEST(!dt.dominates(1, 3));
    TEST(!dt.dominates(2, 2));
    TEST(!dt.dominates(3, 3));
    TEST(!dt.dominates(1, 0));
    TEST(!dt.dominates(2, 0));
    TEST(!dt.dominates(2, 1));
    TEST(!dt.dominates(3, 2));
    TEST(!dt.dominates(3, 1));
    TEST(!dt.dominates(3, 0));

    return true;
}

bool testPostDominance1(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i32 %x)) (
            (!0 ((br !1)))
            (!1 ((br !2)))
            (!2 ((br !3)))
            (!3 ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &pdt = DominatorTree(cfg, DominatorTree::Kind::POSTDOM);

    static const Node &n0 = Node(0, "!0", std::vector<Node>());
    static const Node &n1 = Node(1, "!1", {n0});
    static const Node &n2 = Node(2, "!2", {n1});
    static const Node &n3 = Node(3, "!3", {n2});

    TEST(pdt.root == n3);
    TEST(pdt.unreachables.empty());

    TEST(pdt.dominates(3, 2));
    TEST(pdt.dominates(3, 1));
    TEST(pdt.dominates(3, 0));
    TEST(pdt.dominates(2, 1));
    TEST(pdt.dominates(2, 0));
    TEST(pdt.dominates(1, 0));

    TEST(!pdt.dominates(0, 0));
    TEST(!pdt.dominates(1, 1));
    TEST(!pdt.dominates(2, 2));
    TEST(!pdt.dominates(3, 3));
    TEST(!pdt.dominates(0, 1));
    TEST(!pdt.dominates(0, 2));
    TEST(!pdt.dominates(0, 3));
    TEST(!pdt.dominates(1, 2));
    TEST(!pdt.dominates(1, 3));
    TEST(!pdt.dominates(2, 3));

    return true;
}

bool testPostDominance2(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i8 %c)) (
            (!0 ((brif %c !1 !2)))
            (!1 ((brif %c !2 !3)))
            (!2 ((brif %c !3 !2)))
            (!3 ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &pdt = DominatorTree(cfg, DominatorTree::Kind::POSTDOM);

    static const Node &n0 = Node(0, "!0", std::vector<Node>());
    static const Node &n1 = Node(1, "!1", std::vector<Node>());
    static const Node &n2 = Node(2, "!2", std::vector<Node>());
    static const Node &n3 = Node(3, "!3", {n0, n1, n2});

    TEST(pdt.root == n3);
    TEST(pdt.unreachables.empty());

    TEST(pdt.dominates(3, 2));
    TEST(pdt.dominates(3, 1));
    TEST(pdt.dominates(3, 0));

    TEST(!pdt.dominates(2, 1));
    TEST(!pdt.dominates(2, 0));
    TEST(!pdt.dominates(1, 0));
    TEST(!pdt.dominates(0, 0));
    TEST(!pdt.dominates(1, 1));
    TEST(!pdt.dominates(2, 2));
    TEST(!pdt.dominates(3, 3));
    TEST(!pdt.dominates(0, 1));
    TEST(!pdt.dominates(0, 2));
    TEST(!pdt.dominates(0, 3));
    TEST(!pdt.dominates(1, 2));
    TEST(!pdt.dominates(1, 3));
    TEST(!pdt.dominates(2, 3));

    return true;
}

bool testPostDominance3(void) {
    using Node = DominatorTree::Node;

    static const Program &p = INSIST_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@f void ((i8 %c)) (
            (!a ((brif %c !b !c)))
            (!b ((br !z)))
            (!c ((br !z)))
            (!d ((brif %c !c !z)))
            (!e ((br !b)))
            (!f ((br !f)))
            (!z ((ret void))))))
    )");

    const ControlFlowGraph &cfg = ControlFlowGraph(p.functions[0].body.value());
    const DominatorTree &pdt = DominatorTree(cfg, DominatorTree::Kind::POSTDOM);

    static const Node &na = Node(0, "!a", std::vector<Node>());
    static const Node &nc = Node(2, "!c", std::vector<Node>());
    static const Node &nd = Node(3, "!d", std::vector<Node>());
    static const Node &ne = Node(4, "!e", std::vector<Node>());
    static const Node &nb = Node(1, "!b", {ne});
    static const Node &nz = Node(6, "!z", {na, nb, nc, nd});
    static const Node &nf = Node(5, "!f", std::vector<Node>());

    TEST(pdt.root == nz);
    const std::vector<Node> &eu = {nf};
    TEST(pdt.unreachables == eu);

    TEST(pdt.dominates(6, 0));
    TEST(pdt.dominates(6, 1));
    TEST(pdt.dominates(6, 2));
    TEST(pdt.dominates(6, 3));
    TEST(pdt.dominates(6, 4));
    TEST(pdt.dominates(1, 4));

    TEST(!pdt.dominates(0, 0));
    TEST(!pdt.dominates(0, 1));
    TEST(!pdt.dominates(0, 2));
    TEST(!pdt.dominates(0, 3));
    TEST(!pdt.dominates(0, 4));
    TEST(!pdt.dominates(0, 5));
    TEST(!pdt.dominates(0, 6));
    TEST(!pdt.dominates(1, 0));
    TEST(!pdt.dominates(1, 1));
    TEST(!pdt.dominates(1, 2));
    TEST(!pdt.dominates(1, 3));
    TEST(!pdt.dominates(1, 5));
    TEST(!pdt.dominates(1, 6));
    TEST(!pdt.dominates(2, 0));
    TEST(!pdt.dominates(2, 1));
    TEST(!pdt.dominates(2, 2));
    TEST(!pdt.dominates(2, 3));
    TEST(!pdt.dominates(2, 4));
    TEST(!pdt.dominates(2, 5));
    TEST(!pdt.dominates(2, 6));
    TEST(!pdt.dominates(3, 0));
    TEST(!pdt.dominates(3, 1));
    TEST(!pdt.dominates(3, 2));
    TEST(!pdt.dominates(3, 3));
    TEST(!pdt.dominates(3, 4));
    TEST(!pdt.dominates(3, 5));
    TEST(!pdt.dominates(3, 6));
    TEST(!pdt.dominates(4, 0));
    TEST(!pdt.dominates(4, 1));
    TEST(!pdt.dominates(4, 2));
    TEST(!pdt.dominates(4, 3));
    TEST(!pdt.dominates(4, 4));
    TEST(!pdt.dominates(4, 5));
    TEST(!pdt.dominates(4, 6));
    TEST(!pdt.dominates(5, 0));
    TEST(!pdt.dominates(5, 1));
    TEST(!pdt.dominates(5, 2));
    TEST(!pdt.dominates(5, 3));
    TEST(!pdt.dominates(5, 4));
    TEST(!pdt.dominates(5, 5));
    TEST(!pdt.dominates(5, 6));


    return true;
}
