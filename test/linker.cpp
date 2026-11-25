#include <optional>

#include "SVFIR/Err.h"
#include "SVFIR/Linker.h"
#include "SVFIR/Pretty.h"
#include "SVFIR/Span.h"
#include "SVFIR/Result.h"
#include "SVFIR/Validation.h"
#include "SVFIR/Variant.h"

#include "test.h"

#include <iostream>

using namespace SVFIR;

bool testLinkErrs(void) {
    static const auto diffSigsErr = [](const Name &fn, const Span &span) {
        return Err(
            "The function '" + fn + "' appears on both the "
            "left and the right with different signatures",
            span
        );
    };

    static const auto doubleFnErr = [](const Name &fn, const Span &span) {
        return Err(
            "Function '" + fn + "' has a body on both "
            "the left and right in the presence of strong linkage.",
            span
        );
    };

    static const auto doubleVarErr = [](const Name &gv, const Span &span) {
        return Err(
            "Global variable '" + gv + "' is initialised "
            "on both the left and right in the presence of strong linkage.",
            span
        );
    };

    const TypedProgram &p1l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions (@f i32 () opaque))
    )");
    const TypedProgram &p1r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@f opaque))
        (functions)
    )");
    const Result<TypedProgram, Err> &ae1 = link(p1l, p1r, Linkage::WEAK);
    const Err &ee1 = Err(
        "'@f' is a function on the left and a global variable on the right.",
        Span(Loc(3, 20), Loc(3, 31))
    );
    TEST(is<Err>(ae1));
    TEST(get<Err>(ae1) == ee1);

    const TypedProgram &p2l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 2)))
        (functions (@f i32 () opaque) (@g void () ((!1 ((ret void))))))
    )");
    const TypedProgram &p2r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions)
    )");
    const Result<TypedProgram, Err> &ae2 = link(p2l, p2r, Linkage::STRONG);
    const Err &ee2 = Err(
        "'@g' is a function on the left and a global variable on the right.",
        Span(Loc(3, 20), Loc(3, 32))
    );
    TEST(is<Err>(ae2));
    TEST(get<Err>(ae2) == ee2);

    const TypedProgram &p3l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions (@bar void ((ptr %1)) opaque))
    )");
    const TypedProgram &p3r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 2)))
        (functions (@f i32 () opaque) (@g void () ((!1 ((ret void))))))
    )");
    const Result<TypedProgram, Err> &ae3 = link(p3l, p3r, Linkage::WEAK);
    const Err &ee3 = Err(
        "'@g' is a global variable on the left and a function on the right.",
        Span(Loc(4, 39), Loc(4, 71))
    );
    TEST(is<Err>(ae3));
    TEST(get<Err>(ae3) == ee3);

    const TypedProgram &p4l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions (@foo void ((ptr %1)) opaque))
    )");
    const TypedProgram &p4r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 2)))
        (functions (@foo i32 ((i32 %x) (i16 %2)) opaque))
    )");
    const Result<TypedProgram, Err> &ae4 = link(p4l, p4r, Linkage::WEAK);
    TEST(is<Err>(ae4));
    TEST(get<Err>(ae4) == diffSigsErr("@foo", Span(Loc(4, 20), Loc(4, 57))));

    const TypedProgram &p5l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions (@foo i32 ((ptr %1)) opaque))
    )");
    const TypedProgram &p5r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 2)))
        (functions (@foo i32 ((ptr %1) ...) ((!1 ((ret (i32 2)))))))
    )");
    const Result<TypedProgram, Err> &ae5 = link(p5l, p5r, Linkage::WEAK);
    TEST(is<Err>(ae5));
    TEST(get<Err>(ae5) == diffSigsErr("@foo", Span(Loc(4, 20), Loc(4, 68))));

    const TypedProgram &p6l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions (@x void () opaque) (@foo void (...) opaque))
    )");
    const TypedProgram &p6r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 2)))
        (functions (@x void () opaque) (@foo i32 (...) ((!1 ((ret (i32 2)))))))
    )");
    const Result<TypedProgram, Err> &ae6 = link(p6l, p6r, Linkage::STRONG);
    TEST(is<Err>(ae6));
    TEST(get<Err>(ae6) == diffSigsErr("@foo", Span(Loc(4, 40), Loc(4, 79))));

    const TypedProgram &p7l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g opaque))
        (functions (@x void () ((!1 ((ret void))))))
    )");
    const TypedProgram &p7r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 2)))
        (functions (@x void () ((!1 ((ret void))))))
    )");
    const Result<TypedProgram, Err> &ae7 = link(p7l, p7r, Linkage::STRONG);
    TEST(is<Err>(ae7));
    TEST(get<Err>(ae7) == doubleFnErr("@x", Span(Loc(4, 20), Loc(4, 52))));

    const TypedProgram &p8l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g opaque))
        (functions (@x void () ((!1 ((ret void))))))
    )");
    const TypedProgram &p8r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 2)))
        (functions (@x void () ((!1 ((br !2))) (!2 ((br !2))))))
    )");
    const Result<TypedProgram, Err> &ae8 = link(p8l, p8r, Linkage::STRONG);
    TEST(is<Err>(ae8));
    TEST(get<Err>(ae8) == doubleFnErr("@x", Span(Loc(4, 20), Loc(4, 64))));

    const TypedProgram &p9l = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 2)))
        (functions)
    )");
    const TypedProgram &p9r = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@g (i32 3)))
        (functions (@x void () opaque))
    )");
    const Result<TypedProgram, Err> &ae9 = link(p9l, p9r, Linkage::STRONG);
    TEST(is<Err>(ae9));
    TEST(get<Err>(ae9) == doubleVarErr("@g", Span(Loc(3, 20), Loc(3, 32))));

    return true;
}

bool testLinker(void) {
    // TODO
    return true;
}
