#include <optional>

#include "Analyses.h"
#include "Err.h"
#include "Parser.h"
#include "Span.h"
#include "Result.h"
#include "Variant.h"

#include "test.h"


using namespace SVFIR;

bool testCheckConstantTypes(void) {
    static const auto source1 = [](const std::string &c) {
        const std::string &ps =
            "(preamble (version 0.0) (source test))\n"
            "(variables (@FOO " + c +  "))\n"
            "(functions)\n";
        const Result<Program, Err> &p = parseString(ps);
        // insist, not TEST, because parsing is not this test's responsibility.
        insist(!is<Err>(p));
        return get<Program>(p);
    };

    static const auto span1 = [](const std::string &c) {
        // Corresponds to source1 above.
        return Span(Loc(2, 18), Loc(2, 18 + c.size()));
    };

    static const auto source2 = [](const std::string &c) {
        const std::string &ps =
            "(preamble (version 0.0) (source test))\n"
            "(variables)\n"
            "(functions (@foo void () ((!x ((store " + c + " null))))))\n";
        const Result<Program, Err> &p = parseString(ps);
        insist(!is<Err>(p));
        return get<Program>(p);
    };

    static const auto span2 = [](const std::string &c) {
        return Span(Loc(3, 39), Loc(3, 39 + c.size()));
    };

    static const auto decErr = [](
        const std::string &dec,
        const std::string &exp,
        const std::string &c,
        const std::string &tc,
        const Span &span
    ) {
        return Err(
            "Constant typed incorrectly. Declared as '" + dec + "' but "
            "expected " + exp + " type for '" + c + "' in '" + tc + "'.",
            span
        );
    };

    static const auto aggErr = [](
        const unsigned int &aggSlots,
        const unsigned int &seqSlots,
        const std::string &tc,
        const Span &span
    ) {
        return Err(
            "Declared sequence constant to have " +
            std::to_string(aggSlots) + " slots but in actuality has " +
            std::to_string(seqSlots) + " slots in '" + tc + "'.",
            span
        );
    };

    const std::string &c1 = "(i32 1.1)";
    const std::optional<Err> &e1 = checkConstantTypes(source1(c1));
    TEST(e1.has_value());
    TEST(e1.value() == decErr("i32", "float", "1.1", c1, span1(c1)));

    const std::string &c2 = "(void 1)";
    const std::optional<Err> &e2 = checkConstantTypes(source2(c2));
    TEST(e2.has_value());
    TEST(e2.value() == decErr("void", "integer", "1", c2, span2(c2)));

    const std::string &c3 = "(i8 null)";
    const std::optional<Err> &e3 = checkConstantTypes(source1(c3));
    TEST(e3.has_value());
    TEST(e3.value() == decErr("i8", "pointer", "null", c3, span1(c3)));

    const std::string &c4 = "(ptr (seq (i32 5)))";
    const std::optional<Err> &e4 = checkConstantTypes(source2(c4));
    TEST(e4.has_value());
    TEST(
        e4.value() ==
        decErr("ptr", "aggregation", "(seq (i32 5))", c4, span2(c4))
    );

    const std::string &c5 = "((agg 3) 5.5)";
    const std::optional<Err> &e5 = checkConstantTypes(source1(c5));
    TEST(e5.has_value());
    TEST(e5.value() == decErr("(agg 3)", "float", "5.5", c5, span1(c5)));

    const std::string &c6 = "((agg 0) (seq (i8 0) (i32 6)))";
    const std::optional<Err> &e6 = checkConstantTypes(source2(c6));
    TEST(e6.has_value());
    TEST(e6.value() == aggErr(0, 2, c6, span2(c6)));

    const std::string &c7 = "((agg 50) (seq))";
    const std::optional<Err> &e7 = checkConstantTypes(source1(c7));
    TEST(e7.has_value());
    TEST(e7.value() == aggErr(50, 0, c7, span1(c7)));

    const std::string &c8 = "((agg 2) (seq (i32 1)))";
    const std::optional<Err> &e8 = checkConstantTypes(source2(c8));
    TEST(e8.has_value());
    TEST(e8.value() == aggErr(2, 1, c8, span2(c8)));

    const std::string &c9 = "((agg 1) (seq (f64 5)))";
    const std::optional<Err> &e9 = checkConstantTypes(source1(c9));
    TEST(e9.has_value());
    TEST(
        e9.value() ==
        decErr("f64", "integer", "5", "(f64 5)", Span(Loc(2, 32), Loc(2, 39)))
    );

    // TODO: correct programs.

    return true;
}

int main(void) {
    bool success = true;
    success &= testCheckConstantTypes();

    if (success) { std::cout << "All validation tests passed." << std::endl; }
    else { std::cout << "Some validation tests failed." << std::endl; }
}
