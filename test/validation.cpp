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
        return insistParse(ps);
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
        return insistParse(ps);
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

bool testCheckTerminatingStmts(void) {
    static const auto nonTerminatorErr =
    [](const std::string &stmt, const std::string &bb, const Span &span) {
        return Err(
            "Non-terminating statement '" + stmt + "' cannot be final "
            "statement of basic block '" + bb + "'.",
            span
        );
    };

    static const auto terminatorErr =
    [](const std::string &stmt, const std::string &bb, const Span &span) {
        return Err(
            "Terminating statement '" + stmt + "' not final statement of "
            "basic block '" + bb + "'.",
            span
        );
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((ret void)))
              (!y ((add %x %y %z) (add %a %b %c))))))
    )";
    const std::optional<Err> &e1 = checkTerminatingStmts(insistParse(src1));
    TEST(e1.has_value());
    TEST(
        e1.value() ==
        nonTerminatorErr("(add %a %b %c)", "!y", Span(Loc(7, 35), Loc(7, 49)))
    );

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((alloc stack %1)))
              (!y ((add %x %y %z) (add %a %b %c))))))
    )";
    const std::optional<Err> &e2 = checkTerminatingStmts(insistParse(src2));
    TEST(e2.has_value());
    TEST(
        e2.value() ==
        nonTerminatorErr("(alloc stack %1)", "!x", Span(Loc(6, 20), Loc(6, 36)))
    );

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x1 ((br !y)))
              (!x2 ((brif (i8 1) !y !z)))
              (!y ((phi %p ((%x !x1) (%x !x2)))))
              (!z ((ret void))))))
    )";
    const std::optional<Err> &e3 = checkTerminatingStmts(insistParse(src3));
    TEST(e3.has_value());
    TEST(
        e3.value() ==
        nonTerminatorErr(
            "(phi %p ((%x !x1) (%x !x2)))", "!y", Span(Loc(8, 20), Loc(8, 48))
        )
    );

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((ret void) (alloc stack %1)))
              (!y ((add %x %y %z) (add %a %b %c))))))
    )";
    const std::optional<Err> &e4 = checkTerminatingStmts(insistParse(src4));
    TEST(e4.has_value());
    TEST(
        e4.value() ==
        terminatorErr("(ret void)", "!x", Span(Loc(6, 20), Loc(6, 30)))
    );

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((br !y)))
              (!y ((br !x) (add %a %b %c))))))
    )";
    const std::optional<Err> &e5 = checkTerminatingStmts(insistParse(src5));
    TEST(e5.has_value());
    TEST(
        e5.value() ==
        terminatorErr("(br !x)", "!y", Span(Loc(7, 20), Loc(7, 27)))
    );

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((br !y)))
              (!y ((add %a %b %c) (br !z)))
              (!z ((or %o (i8 1) (i8 2)) (brif %o !y !z) (ret void))))))
    )";
    const std::optional<Err> &e6 = checkTerminatingStmts(insistParse(src6));
    TEST(e6.has_value());
    TEST(
        e6.value() ==
        terminatorErr("(brif %o !y !z)", "!z", Span(Loc(8, 42), Loc(8, 57)))
    );

    // TODO: correct programs.

    return true;
}

bool testCheckDuplicateDecs(void) {
    static const auto globalVarErr =
    [](const std::string &name, const Span &span) {
        return Err(
            "Global variable name '" + name + "' previously declared", span
        );
    };

    static const auto fnErr =
    [](const std::string &name, const Span &span) {
        return Err("Function name '" + name + "' previously declared", span);
    };

    static const auto paramErr =
    [](const std::string &name, const Span &span) {
        return Err("Parameter '" + name + "' previously declared.", span);
    };

    static const auto localVarErr =
    [](const std::string &name, const Span &span) {
        return Err("Variable '" + name + "' preivously declared.", span);
    };

    static const auto blockErr =
    [](const std::string &name, const Span &span) {
        return Err("Block '" + name + "' previously declared.", span);
    };

    // Globals.

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque) (@foo (i32 2)))
        (functions)
    )";
    const std::optional<Err> &e1 = checkDuplicateDecs(insistParse(src1));
    TEST(e1.has_value());
    TEST(e1.value() == globalVarErr("@foo", Span(Loc(3, 35), Loc(3, 39))));

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque) (@bar (i32 2)))
        (functions (@baz void () opaque) (@bar void () ((!x ((ret void))))))
    )";
    const std::optional<Err> &e2 = checkDuplicateDecs(insistParse(src2));
    TEST(e2.has_value());
    TEST(e2.value() == fnErr("@bar", Span(Loc(4, 43), Loc(4, 47))));

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo ((agg 0) (seq))))
        (functions
          (@bar void () ((!x ((ret void)))))
          (@bar void () ((!x ((ret void))))))
    )";
    const std::optional<Err> &e3 = checkDuplicateDecs(insistParse(src3));
    TEST(e3.has_value());
    TEST(e3.value() == fnErr("@bar", Span(Loc(6, 12), Loc(6, 16))));

    // Locals

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque))
        (functions
          (@bar void ((i32 %foo) (i64 %bar) (i16 %bar)) ((!x ((ret void))))))
    )";
    const std::optional<Err> &e4 = checkDuplicateDecs(insistParse(src4));
    TEST(e4.has_value());
    TEST(e4.value() == paramErr("%bar", Span(Loc(5, 50), Loc(5, 54))));

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque))
        (functions
          (@bar void ((i32 %foo)) ((!foo ((alloc stack %foo))))))
    )";
    const std::optional<Err> &e5 = checkDuplicateDecs(insistParse(src5));
    TEST(e5.has_value());
    TEST(e5.value() == localVarErr("%foo", Span(Loc(5, 56), Loc(5, 60))));

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn i32 () ((!foo ((alloc stack %foo) (not %foo (i32 550)))))))
    )";
    const std::optional<Err> &e6 = checkDuplicateDecs(insistParse(src6));
    TEST(e6.has_value());
    TEST(e6.value() == localVarErr("%foo", Span(Loc(5, 55), Loc(5, 59))));

    const std::string &src7 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn i32 () (
            (!foo ((alloc stack %foo) (not %bar (i32 550))))
            (!bar ((blackhole pure (f16 %foo)))))))
    )";
    const std::optional<Err> &e7 = checkDuplicateDecs(insistParse(src7));
    TEST(e7.has_value());
    TEST(e7.value() == localVarErr("%foo", Span(Loc(7, 41), Loc(7, 45))));

    const std::string &src8 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn1 i32 () (
            (!foo ((alloc stack %foo) (not %bar (i32 550))))
            (!bar ((blackhole pure (f16 %baz))))))
          (@fn2 void ((i32 %foo)) (
            (!foo ((ret %foo)))
            (!bar ((load (f16 %foo) null))))))
    )";
    const std::optional<Err> &e8 = checkDuplicateDecs(insistParse(src8));
    TEST(e8.has_value());
    TEST(e8.value() == localVarErr("%foo", Span(Loc(10, 31), Loc(10, 35))));

    const std::string &src9 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn1 i32 () (
            (!foo ((alloc stack %foo) (not %bar (i32 550))))
            (!bar ((blackhole pure (f16 %baz))))))
          (@fn2 void ((i32 %foo)) (
            (!foo ((brif %foo !bar !bar)))
            (!bar ((field %fld null 0) (convert %fld %fld i64))))))
    )";
    const std::optional<Err> &e9 = checkDuplicateDecs(insistParse(src9));
    TEST(e9.has_value());
    TEST(e9.value() == localVarErr("%fld", Span(Loc(10, 49), Loc(10, 53))));

    const std::string &src10 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn1 i32 ((i32 %foo1)) (
            (!foo ((alloc stack %foo2) (not %bar (i32 550))))
            (!bar ((blackhole pure (f16 %baz))))))
          (@fn2 void ((i32 %foo) (ptr %x) (ptr %x)) (
            (!foo ((brif %foo !bar !bar)))
            (!bar ((field %fld null 0) (ret %fld))))))
    )";
    const std::optional<Err> &e10 = checkDuplicateDecs(insistParse(src10));
    TEST(e10.has_value());
    TEST(e10.value() == paramErr("%x", Span(Loc(8, 48), Loc(8, 50))));

    // Blocks.

    const std::string &src11 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () ((!1 ((ret void))) (!1 ((ret void)))))))";
    const std::optional<Err> &e11 = checkDuplicateDecs(insistParse(src11));
    TEST(e11.has_value());
    TEST(e11.value() == blockErr("!1", Span(Loc(5, 44), Loc(5, 46))));

    const std::string &src12 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn1 void () ((!1 ((ret void))) (!2 ((ret void)))))
          (@fn2 void () (
            (!1 ((ret void)))
            (!x ((ret void)))
            (!2 ((ret void)))
            (!x ((ret void)))))))";
    const std::optional<Err> &e12 = checkDuplicateDecs(insistParse(src12));
    TEST(e12.has_value());
    TEST(e12.value() == blockErr("!x", Span(Loc(10, 14), Loc(10, 16))));

    // TODO: correct programs.

    return true;
}

int main(void) {
    bool success = true;
    success &= testCheckConstantTypes();
    success &= testCheckTerminatingStmts();
    success &= testCheckDuplicateDecs();

    if (success) { std::cout << "All validation tests passed." << std::endl; }
    else { std::cout << "Some validation tests failed." << std::endl; }
}
