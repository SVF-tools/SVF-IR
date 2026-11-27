#include "SVFIR/Pretty.h"
#include "SVFIR/UnifyReturns.h"

#include "test.h"

bool testUnifyReturns1(void) {
    const TypedProgram &tp = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables)
        (functions
          (@foo void ((i8 %c)) (
            (!0 ((ret void)))
            (!1 (
              (alloc heap %x)
              (ret void)))
            (!2 (
              (blackhole pure (i32 %y))
              (brif %c !1 !3)))
            (!3 (
              (add %z1 (i32 2) (i32 3))
              (sub %z2 %z1 (i32 3))
              (ret void)))))
          (@bar void () ((!0 ((ret void))))))
    )");

    const TypedProgram &utp = unifyReturns(tp);
    TEST(
        pretty(utp) ==
        strip(R"(
            (preamble
              (version 0.0)
              (source hand))
            (variables)
            (functions
              (@foo void ((i8 %c)) (
                (!0 (
                  (br !unified-ret)))
                (!1 (
                  (alloc heap %x [ptr])
                  (br !unified-ret)))
                (!2 (
                  (blackhole pure (i32 %y))
                  (brif %c [i8] !1 !3)))
                (!3 (
                  (add %z1 [i32] (i32 2) (i32 3))
                  (sub %z2 [i32] %z1 [i32] (i32 3))
                  (br !unified-ret)))
                (!unified-ret (
                  (ret void)))))
              (@bar void () (
                (!0 (
                  (ret void))))))
        )")
    );

    return true;
}

bool testUnifyReturns2(void) {
    const TypedProgram &tp = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 200)))
        (functions
          (@foo i32 ((i32 %a) (i32 %b)) (
            (!0 (
              (convert %c %a i8)
              (brif %c !1 !2)))
            (!1 (
              (alloc stack %x)
              (load (i32 %ix) %x)
              (ret %ix)))
            (!2 (
              (phi %p ((%a !0)))
              (ret %p)))
            (!3 (
              (add %z1 (i32 2) (i32 3))
              (sub %z2 %z1 (i32 3))
              (ret (i32 5))))
            (!? (
              (load (i32 %iv) @v)
              (ret %iv))))))
    )");

    const TypedProgram &utp = unifyReturns(tp);
    TEST(
        pretty(utp) ==
        strip(R"(
            (preamble
              (version 0.0)
              (source hand))
            (variables
              (@v (i32 200)))
            (functions
              (@foo i32 ((i32 %a) (i32 %b)) (
                (!0 (
                  (convert %c [i8] %a [i32] i8)
                  (brif %c [i8] !1 !2)))
                (!1 (
                  (alloc stack %x [ptr])
                  (load (i32 %ix) %x [ptr])
                  (br !unified-ret)))
                (!2 (
                  (phi %p [i32] ((%a [i32] !0)))
                  (br !unified-ret)))
                (!3 (
                  (add %z1 [i32] (i32 2) (i32 3))
                  (sub %z2 [i32] %z1 [i32] (i32 3))
                  (br !unified-ret)))
                (!? (
                  (load (i32 %iv) @v)
                  (br !unified-ret)))
                (!unified-ret (
                  (phi %unified-ret [i32] ((%ix [i32] !1) (%p [i32] !2) ((i32 5) !3) (%iv [i32] !?)))
                  (ret %unified-ret [i32]))))))
        )")
    );

    return true;
}

bool testUnifyReturns3(void) {
    const std::string &p = strip(R"(
        (preamble
          (version 0.0)
          (source hand))
        (variables)
        (functions
          (@foo i32 () (
            (!0 (
              (ret (i32 2))))))
          (@bar i32 () (
            (!0 (
              (br !1)))
            (!1 (
              (br !1))))))
    )");

    const TypedProgram &tp = INSIST_TYPED_PARSE(p);
    const TypedProgram &utp = unifyReturns(tp);
    TEST(pretty(utp.program) == p);

    return true;
}

bool testUnifyReturns4(void) {
    const TypedProgram &tp = INSIST_TYPED_PARSE(R"(
        (preamble (version 0.0) (source hand))
        (variables (@v (i32 200)))
        (functions
          (@foo ptr () (
            (!0 ((ret @v)))
            (!1 ((ret (ptr null)))))))
    )");

    const TypedProgram &utp = unifyReturns(tp);

    TEST(
        pretty(utp) ==
        strip(R"(
            (preamble
              (version 0.0)
              (source hand))
            (variables
              (@v (i32 200)))
            (functions
              (@foo ptr () (
                (!0 (
                  (br !unified-ret)))
                (!1 (
                  (br !unified-ret)))
                (!unified-ret (
                  (phi %unified-ret [ptr] ((@v !0) ((ptr null) !1)))
                  (ret %unified-ret [ptr]))))))
        )")
    );

    return true;
}
