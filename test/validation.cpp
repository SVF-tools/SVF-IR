#include <optional>
#include <cstdlib>

#include "Analyses.h"
#include "Err.h"
#include "Span.h"
#include "SVFIR.h"
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
    const Err &ee1 = decErr("i32", "float", "1.1", c1, span1(c1));
    const std::optional<Err> &ae1 = checkConstantTypes(source1(c1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &c2 = "(void 1)";
    const Err &ee2 = decErr("void", "integer", "1", c2, span2(c2));
    const std::optional<Err> &ae2 = checkConstantTypes(source2(c2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &c3 = "(i8 null)";
    const Err &ee3 = decErr("i8", "pointer", "null", c3, span1(c3));
    const std::optional<Err> &ae3 = checkConstantTypes(source1(c3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    const std::string &c4 = "(ptr (seq (i32 5)))";
    const Err &ee4 =
        decErr("ptr", "aggregation", "(seq (i32 5))", c4, span2(c4));
    const std::optional<Err> &ae4 = checkConstantTypes(source2(c4));
    TEST(ae4.has_value() && ae4.value() == ee4);

    const std::string &c5 = "((agg 3) 5.5)";
    const Err &ee5 = decErr("(agg 3)", "float", "5.5", c5, span1(c5));
    const std::optional<Err> &ae5 = checkConstantTypes(source1(c5));
    TEST(ae5.has_value() && ae5.value() == ee5);

    const std::string &c6 = "((agg 0) (seq (i8 0) (i32 6)))";
    const Err &ee6 = aggErr(0, 2, c6, span2(c6));
    const std::optional<Err> &ae6 = checkConstantTypes(source2(c6));
    TEST(ae6.has_value() && ae6.value() == ee6);

    const std::string &c7 = "((agg 50) (seq))";
    const Err &ee7 = aggErr(50, 0, c7, span1(c7));
    const std::optional<Err> &ae7 = checkConstantTypes(source1(c7));
    TEST(ae7.has_value() && ae7.value() == ee7);

    const std::string &c8 = "((agg 2) (seq (i32 1)))";
    const Err &ee8 = aggErr(2, 1, c8, span2(c8));
    const std::optional<Err> &ae8 = checkConstantTypes(source2(c8));
    TEST(ae8.has_value() && ae8.value() == ee8);

    const std::string &c9 = "((agg 1) (seq (f64 5)))";
    const Err &ee9 =
        decErr("f64", "integer", "5", "(f64 5)", Span(Loc(2, 32), Loc(2, 39)));
    const std::optional<Err> &ae9 = checkConstantTypes(source1(c9));
    TEST(ae9.has_value() && ae9.value() == ee9);

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
              (!y ((add %y (i8 1) (i8 2)) (not %z (i8 1)))))))
    )";
    const Err &ee1 =
        nonTerminatorErr("(not %z (i8 1))", "!y", Span(Loc(7, 43), Loc(7, 58)));
    const std::optional<Err> &ae1 = checkTerminatingStmts(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((alloc stack %1)))
              (!y ((add %a (f16 1.0) (f16 2.2)))))))
    )";
    const Err &ee2 = nonTerminatorErr(
            "(alloc stack %1)", "!x", Span(Loc(6, 20), Loc(6, 36))
        );
    const std::optional<Err> &ae2 = checkTerminatingStmts(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

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
    const Err &ee3 = nonTerminatorErr(
            "(phi %p ((%x !x1) (%x !x2)))", "!y", Span(Loc(8, 20), Loc(8, 48))
        );
    const std::optional<Err> &ae3 = checkTerminatingStmts(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((i32 %x)) (
              (!x ((ret void) (alloc stack %1) (ret void)))
              (!y ((add %a %x %x) (add %a %x %x) (ret void))))))
    )";
    const Err &ee4 =
        terminatorErr("(ret void)", "!x", Span(Loc(6, 20), Loc(6, 30)));
    const std::optional<Err> &ae4 = checkTerminatingStmts(insistParse(src4));
    TEST(ae4.has_value() && ae4.value() == ee4);

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((br !y)))
              (!y ((br !x) (add %a %b %c))))))
    )";
    const Err &ee5 =
        terminatorErr("(br !x)", "!y", Span(Loc(7, 20), Loc(7, 27)));
    const std::optional<Err> &ae5 = checkTerminatingStmts(insistParse(src5));
    TEST(ae5.has_value() && ae5.value() == ee5);

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@foo void ((ptr %x)) (
              (!x ((br !y)))
              (!y ((add %a %b %c) (br !z)))
              (!z ((or %o (i8 1) (i8 2)) (brif %o !y !z) (ret void))))))
    )";
    const Err &ee6 =
        terminatorErr("(brif %o !y !z)", "!z", Span(Loc(8, 42), Loc(8, 57)));
    const std::optional<Err> &ae6 = checkTerminatingStmts(insistParse(src6));
    TEST(ae6.has_value() && ae6.value() == ee6);

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
    const Err &ee1 = globalVarErr("@foo", Span(Loc(3, 35), Loc(3, 39)));
    const std::optional<Err> &ae1 = checkDuplicateDecs(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque) (@bar (i32 2)))
        (functions (@baz void () opaque) (@bar void () ((!x ((ret void))))))
    )";
    const Err &ee2 = fnErr("@bar", Span(Loc(4, 43), Loc(4, 47)));
    const std::optional<Err> &ae2 = checkDuplicateDecs(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo ((agg 0) (seq))))
        (functions
          (@bar void () ((!x ((ret void)))))
          (@bar void () ((!x ((ret void))))))
    )";
    const Err &ee3 = fnErr("@bar", Span(Loc(6, 12), Loc(6, 16)));
    const std::optional<Err> &ae3 = checkDuplicateDecs(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    // Locals

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque))
        (functions
          (@bar void ((i32 %foo) (i64 %bar) (i16 %bar)) ((!x ((ret void))))))
    )";
    const Err &ee4 = paramErr("%bar", Span(Loc(5, 50), Loc(5, 54)));
    const std::optional<Err> &ae4 = checkDuplicateDecs(insistParse(src4));
    TEST(ae4.has_value() && ae4.value() == ee4);

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque))
        (functions
          (@bar void ((i32 %foo)) ((!foo ((alloc stack %foo))))))
    )";
    const Err &ee5 = localVarErr("%foo", Span(Loc(5, 56), Loc(5, 60)));
    const std::optional<Err> &ae5 = checkDuplicateDecs(insistParse(src5));
    TEST(ae5.has_value() && ae5.value() == ee5);

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn i32 () ((!foo ((alloc stack %foo) (not %foo (i32 550)))))))
    )";
    const Err &ee6 = localVarErr("%foo", Span(Loc(5, 55), Loc(5, 59)));
    const std::optional<Err> &ae6 = checkDuplicateDecs(insistParse(src6));
    TEST(ae6.has_value() && ae6.value() == ee6);

    const std::string &src7 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn i32 () (
            (!foo ((alloc stack %foo) (not %bar (i32 550))))
            (!bar ((blackhole pure (f16 %foo)))))))
    )";
    const Err &ee7 = localVarErr("%foo", Span(Loc(7, 41), Loc(7, 45)));
    const std::optional<Err> &ae7 = checkDuplicateDecs(insistParse(src7));
    TEST(ae7.has_value() && ae7.value() == ee7);

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
    const Err &ee8 = localVarErr("%foo", Span(Loc(10, 31), Loc(10, 35)));
    const std::optional<Err> &ae8 = checkDuplicateDecs(insistParse(src8));
    TEST(ae8.has_value() && ae8.value() == ee8);

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
    const Err &ee9 = localVarErr("%fld", Span(Loc(10, 49), Loc(10, 53)));
    const std::optional<Err> &ae9 = checkDuplicateDecs(insistParse(src9));
    TEST(ae9.has_value() && ae9.value() == ee9);

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
    const Err &ee10 = paramErr("%x", Span(Loc(8, 48), Loc(8, 50)));
    const std::optional<Err> &ae10 = checkDuplicateDecs(insistParse(src10));
    TEST(ae10.has_value() && ae10.value() == ee10);

    // Blocks.

    const std::string &src11 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () ((!1 ((ret void))) (!1 ((ret void)))))))";
    const Err &ee11 = blockErr("!1", Span(Loc(5, 44), Loc(5, 46)));
    const std::optional<Err> &ae11 = checkDuplicateDecs(insistParse(src11));
    TEST(ae11.has_value() && ae11.value() == ee11);

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
    const Err &ee12 = blockErr("!x", Span(Loc(10, 14), Loc(10, 16)));
    const std::optional<Err> &ae12 = checkDuplicateDecs(insistParse(src12));
    TEST(ae12.has_value() && ae12.value() == ee12);

    // TODO: correct programs.

    return true;
}

bool testCheckVarargStmts(void) {
    static const auto varargErr =
    [](const std::string &stmt, const std::string &fn, const Span &span) {
        return Err(
            "vararg statement '" + stmt +
            "' in non-vararg function '" + fn + "'.",
            span
        );
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (@fn void () ((!1 ((vararg (i32 %x) 1))))))
    )";
    const Err &ee1 = varargErr("(vararg (i32 %x) 1)", "@fn", Span(Loc(4, 39), Loc(4, 58)));
    const std::optional<Err> &ae1 = checkVarargStmts(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@o void () opaque)
          (@fn (agg 0) ((i32 %p) (ptr %1)) (
            (!1 ((load (ptr %1) null)))
            (!2 ((vararg (ptr %1) 0))))))
    )";
    const Err &ee2 = varargErr("(vararg (ptr %1) 0)", "@fn", Span(Loc(8, 18), Loc(8, 37)));
    const std::optional<Err> &ae2 = checkVarargStmts(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    // TODO: correct programs.

    return true;
}

bool testCheckUndeclaredBlocks(void) {
    static const auto undecErr =
    [](const std::string &name, const Span &span) {
        return Err("Block '" + name + "' undeclared.", span);
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (@fn void () ((!1 ((br !2))))))
    )";
    const Err &ee1 = undecErr("!2", Span(Loc(4, 43), Loc(4, 45)));
    const std::optional<Err> &ae1 = checkUndeclaredBlocks(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (@fn void () ((!1 ((brif (i8 0) !1 !2))))))
    )";
    const Err &ee2 = undecErr("!2", Span(Loc(4, 55), Loc(4, 57)));
    const std::optional<Err> &ae2 = checkUndeclaredBlocks(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () ((!x ((ret void)))))
          (@fn void () ((!y ((phi %x (((i32 5) !y) ((i32 4) !x))))))))
    )";
    const Err &ee3 = undecErr("!x", Span(Loc(6, 61), Loc(6, 63)));
    const std::optional<Err> &ae3 = checkUndeclaredBlocks(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    // TODO: correct programs.

    return true;
}

bool testCheckUndeclaredGlobals(void) {
    static const auto undecErr =
    [](const std::string &name, const Span &span) {
        return Err("Global '" + name + "' undeclared.", span);
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo @bar))
        (functions)
    )";
    const Err &ee1 = undecErr("@bar", Span(Loc(3, 26), Loc(3, 30)));
    const std::optional<Err> &ae1 = checkUndeclaredGlobals(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo opaque))
        (functions
          (@fn void () ( (!1 ((store (ptr null) @foo) (load (i32 %a) @foo2))))))
    )";
    const Err &ee2 = undecErr("@foo2", Span(Loc(5, 70), Loc(5, 75)));
    const std::optional<Err> &ae2 = checkUndeclaredGlobals(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables (@foo @foo))
        (functions
          (@fn ptr () ( (!1 ((store (ptr null) @foo) (ret @fn)))))
          (@fnn void () ( (!2 ((call void @void ((i32 2))))))))
    )";
    const Err &ee3 = undecErr("@void", Span(Loc(6, 43), Loc(6, 48)));
    const std::optional<Err> &ae3 = checkUndeclaredGlobals(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    // TODO: correct programs.

    return true;
}

bool testCheckEntryPredecessors(void) {
    static const auto predErr =
    [](
        const std::string &entry,
        const std::string &fn,
        const std::string &stmt,
        const Span &span
    ) {
        return Err(
            "The entry block '" + entry + "' of function '" + fn + "' is "
            "targeted by '" + stmt + "'. "
            "Entry blocks cannot have predecessors.",
            span
        );
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@f void () ((!1 ((br !1))))))
    )";
    const Err &ee1 =
        predErr("!1", "@f", "(br !1)", Span(Loc(5, 29), Loc(5, 36)));
    const std::optional<Err> &ae1 = checkEntryPredecessors(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@f void () ((!1 ((ret void)))))
          (@g void () ((!x ((ret void))) (!y ((brif (i8 1) !y !x))))))
    )";
    const Err &ee2 = predErr(
            "!x", "@g", "(brif (i8 1) !y !x)", Span(Loc(6, 47), Loc(6, 66))
        );
    const std::optional<Err> &ae2 = checkEntryPredecessors(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@f void () ((!1 ((ret void)))))
          (@g void () ((!x ((ret void))) (!y ((brif (i8 1) !x !y))))))
    )";
    const Err &ee3 = predErr(
        "!x", "@g", "(brif (i8 1) !x !y)", Span(Loc(6, 47), Loc(6, 66))
    );
    const std::optional<Err> &ae3 = checkEntryPredecessors(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@f void () ((!1 ((br !2))) (!2 ((ret void)))))
          (@g void () ((!2 ((ret void))))))
    )";
    const std::optional<Err> &ae4 = checkEntryPredecessors(insistParse(src4));
    TEST(!ae4.has_value());

    // TODO: correct programs.

    return true;
}

bool testCheckPhiStmtArgs(void) {
    static const auto dupPredErr =
    [](const std::string &block, const std::string &phi, const Span &span) {
        return Err(
            "Duplicate argument for predecessor block '" + block +
            "' in phi statement '" + phi + "'.",
            span
        );
    };

    static const auto notPredErr =
    [](const std::string &block, const std::string &phi, const Span &span) {
        return Err(
            "Argument block '" + block + "' is not a predecessor block "
            "in phi statement '" + phi + "'.",
            span
        );
    };

    static const auto incomprehensiveErr =
    [](
        const std::string &uncovered,
        // Whether uncovered refers to many or just one.
        const bool plural,
        const std::string &phi,
        const Span &span
    ) {
        return Err(
            "No corresponding argument for predecessor block" +
            (plural ? "s " + uncovered : " " + uncovered) +
            " in phi statement '" + phi + "'.",
            span
        );
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () (
            (!1 ((br !2)))
            (!2 ((phi %x (((i8 1) !1) ((i8 2) !1))) (ret void))))))
    )";
    const Err &ee1 = dupPredErr(
        "!1", "(phi %x (((i8 1) !1) ((i8 2) !1)))", Span(Loc(7, 39), Loc(7, 50))
    );
    const std::optional<Err> &ae1 = checkPhiStmtArgs(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () (
            (!1 ((br !2)))
            (!2 ((phi %x (((i8 1) !1) ((i8 2) !2) ((i8 3) !1))) (br !2))))))
    )";
    const Err &ee2 = dupPredErr(
        "!1",
        "(phi %x (((i8 1) !1) ((i8 2) !2) ((i8 3) !1)))",
        Span(Loc(7, 51), Loc(7, 62))
    );
    const std::optional<Err> &ae2 = checkPhiStmtArgs(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn void () (
            (!1 ((phi %a (((ptr null) !1))) (ret void))))))
    )";
    const Err &ee3 = notPredErr(
        "!1", "(phi %a (((ptr null) !1)))", Span(Loc(6, 27), Loc(6, 42))
    );
    const std::optional<Err> &ae3 = checkPhiStmtArgs(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn ptr ((ptr %x)) (
            (!1 ((br !2)))
            (!2 ((br !3)))
            (!3 ((phi %y ((%x !2) (%x !1))) (ret %y))))))
    )";
    const Err &ee4 = notPredErr(
        "!1", "(phi %y ((%x !2) (%x !1)))", Span(Loc(8, 35), Loc(8, 42))
    );
    const std::optional<Err> &ae4 = checkPhiStmtArgs(insistParse(src4));
    TEST(ae4.has_value() && ae4.value() == ee4);

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn ptr ((ptr %x)) (
            (!1 ((br !3)))
            (!2 ((br !3)))
            (!3 ((phi %y ((%x !2))) (ret %y))))))
    )";
    const Err &ee5 = incomprehensiveErr(
        "'!1'", false, "(phi %y ((%x !2)))", Span(Loc(8, 18), Loc(8, 36))
    );
    const std::optional<Err> &ae5 = checkPhiStmtArgs(insistParse(src5));
    TEST(ae5.has_value() && ae5.value() == ee5);

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions
          (@fn ptr ((ptr %x)) (
            (!1 ((brif (i8 1) !a !2)))
            (!a ((brif (i8 1) !b !2)))
            (!b ((br !2)))
            (!c ((brif (i8 1) !2 !2)))
            (!2 ((phi %y ((%x !1))) (ret %y))))))
    )";
    const Err &ee6 = incomprehensiveErr(
        "'!c', '!b', '!a'",
        true,
        "(phi %y ((%x !1)))",
        Span(Loc(10, 18), Loc(10, 36))
    );
    const std::optional<Err> &ae6 = checkPhiStmtArgs(insistParse(src6));
    TEST(ae6.has_value() && ae6.value() == ee6);

    // TODO: correct programs.

    return true;
}

bool testCheckLocalDefBeforeUse(void) {
    static const auto useErr =
    [](const std::string &name, const std::string &stmt, const Span &span) {
        return Err(
            "Local variable '" + name + "' used "
            "before definition in '" + stmt + "'.",
            span
        );
    };

    static const auto phiErr =
    [](const std::string &name, const std::string &pred, const Span &span) {
        return Err(
            "Local variable '" + name + "' in phi argument was not defined "
            "in or strictly before predecessor '" + pred + "'.",
            span
        );
    };

    const std::string &src1 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (@f i32 () ((!1 ((ret %x))))))
    )";
    const Err &ee1 = useErr("%x", "(ret %x)", Span(Loc(4, 42), Loc(4, 44)));
    const std::optional<Err> &ae1 = checkLocalDefBeforeUse(insistParse(src1));
    TEST(ae1.has_value() && ae1.value() == ee1);

    const std::string &src2 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f ptr () (
            (!1 ((load (i32 %1) %x) (br !2)))
            (!2 ((alloc stack %x) (ret %x))))))
    )";
    const Err &ee2 =
        useErr("%x", "(load (i32 %1) %x)", Span(Loc(6, 33), Loc(6, 35)));
    const std::optional<Err> &ae2 = checkLocalDefBeforeUse(insistParse(src2));
    TEST(ae2.has_value() && ae2.value() == ee2);

    const std::string &src3 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %a %1) (br !3)))
            (!3 ((ret %a))))))
    )";
    const Err &ee3 = useErr("%a", "(ret %a)", Span(Loc(8, 23), Loc(8, 25)));
    const std::optional<Err> &ae3 = checkLocalDefBeforeUse(insistParse(src3));
    TEST(ae3.has_value() && ae3.value() == ee3);

    const std::string &src4 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %a %1) (br !3)))
            (!3 ((ret %1))))))
    )";
    const std::optional<Err> &ae4 = checkLocalDefBeforeUse(insistParse(src4));
    TEST(!ae4.has_value());

    const std::string &src5 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %a %1) (br !3)))
            (!3 ((phi %z ((%1 !2) (%1 !1))) (ret (i32 2)))))))
    )";
    const std::optional<Err> &ae5 = checkLocalDefBeforeUse(insistParse(src5));
    TEST(!ae5.has_value());

    const std::string &src6 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %a %1) (br !3)))
            (!3 ((phi %z ((%a !2) (%a !1))) (ret (i32 2)))))))
    )";
    const Err &ee6 = phiErr("%a", "!1", Span(Loc(8, 35), Loc(8, 42)));
    const std::optional<Err> &ae6 = checkLocalDefBeforeUse(insistParse(src6));
    TEST(ae6.has_value() && ae6.value() == ee6);

    const std::string &src7 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (br !3)))
            (!2 ((not %2 (i32 3)) (br !3)))
            (!3 ((phi %x ((%1 !1) (%2 !2))) (br !4)))
            (!4 ((phi %y ((%2 !3))) (ret %y))))))
    )";
    const Err &ee7 = phiErr("%2", "!3", Span(Loc(9, 27), Loc(9, 34)));
    const std::optional<Err> &ae7 = checkLocalDefBeforeUse(insistParse(src7));
    TEST(ae7.has_value() && ae7.value() == ee7);

    const std::string &src8 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %2 (i32 3)) (br !3)))
            (!3 ((phi %x ((%1 !1) (%2 !2))) (br !4)))
            (!4 ((phi %y ((%2 !3))) (ret %y))))))
    )";
    const Err &ee8 = phiErr("%2", "!3", Span(Loc(9, 27), Loc(9, 34)));
    const std::optional<Err> &ae8 = checkLocalDefBeforeUse(insistParse(src8));
    TEST(ae8.has_value() && ae8.value() == ee8);

    const std::string &src9 = R"(
        (preamble (version 0.0) (source test))
        (variables)
        (functions (
          @f i32 () (
            (!1 ((load (i32 %1) null) (brif %1 !2 !3)))
            (!2 ((not %2 (i32 3)) (br !3)))
            (!3 ((phi %x ((%1 !1) (%2 !2))) (br !4)))
            (!4 ((phi %y ((%1 !3))) (ret %y))))))
    )";
    const std::optional<Err> &ae9 = checkLocalDefBeforeUse(insistParse(src9));
    TEST(!ae9.has_value());

    // TODO: correct programs.

    return true;
}

int main(void) {
    bool success = true;
    success &= testCheckConstantTypes();
    success &= testCheckTerminatingStmts();
    success &= testCheckDuplicateDecs();
    success &= testCheckVarargStmts();
    success &= testCheckUndeclaredBlocks();
    success &= testCheckUndeclaredGlobals();
    success &= testCheckEntryPredecessors();
    success &= testCheckPhiStmtArgs();
    success &= testCheckLocalDefBeforeUse();

    if (success) { std::cout << "All validation tests passed." << std::endl; }
    else { std::cout << "Some validation tests failed." << std::endl; }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
