#include <cstdlib>

#include "SVFIR/Err.h"
#include "SVFIR/Pretty.h"
#include "SVFIR/Result.h"
#include "SVFIR/Variant.h"

#include "SVFIR/ControlFlowGraph.h"
#include "SVFIR/DominatorTree.h"

#include "test.h"

using namespace SVFIR;

bool roundtrip(void) {
    const std::string program1 = strip(R"(
        (preamble
          (version 0.0)
          (source hand))
        (variables)
        (functions)
    )");
    const Result<Program, Err> res1 = parseString(program1);
    TEST(!is<Err>(res1));
    TEST(pretty(getVal(res1)) == program1);

    const std::string &program2 = strip(R"(
        (preamble
          (version 0.0)
          (source hand))
        (variables
          (@MAX_LENGTH (i32 10)))
        (functions
          (@count i32 ((ptr %0)) (
            (!1 (
              (alloc stack %2 (md (type ptr)))
              (alloc stack %3 (md (type i32)))
              (alloc stack %4 (md (type ptr)))
              (store %0 %2)
              (store (i32 0) %3)
              (load (ptr %5) %2)
              (store %5 %4)
              (br !2)))
            (!2 (
              (load (ptr %7) %4)
              (cmp != %8 %7 (ptr null))
              (brif %8 !3 !4)))
            (!3 (
              (load (i32 %10) %3)
              (add %11 %10 (i32 1))
              (store %11 %3)
              (load (ptr %12) %4)
              (field %13 %12 1)
              (load (ptr %14) %13)
              (store %14 %4)
              (br !2)))
            (!4 (
              (load (i32 %16) %3)
              (ret %16)))))
          (@append void ((ptr %0) (i32 %1)) (
            (!1 (
              (alloc stack %3 (md (type ptr)))
              (alloc stack %4 (md (type i32)))
              (alloc stack %5 (md (type ptr)))
              (alloc stack %6 (md (type ptr)))
              (store %0 %3)
              (store %1 %4)
              (load (ptr %7) %3)
              (call (i32 %8) @count (%7))
              (cmp = %9 %8 (i32 10))
              (brif %9 !10 !11)))
            (!10 (
              (br !34)))
            (!11 (
              (load (ptr %12) %3)
              (store %12 %5)
              (br !13)))
            (!13 (
              (load (ptr %14) %5)
              (cmp != %15 %14 (ptr null))
              (brif %15 !16 !34)))
            (!16 (
              (load (ptr %17) %5)
              (field %18 %17 1)
              (load (ptr %19) %18)
              (cmp = %20 %19 (ptr null))
              (brif %20 !21 !30)))
            (!21 (
              (alloc heap %22 (md (size 16)))
              (store %22 %7)
              (load (i32 %23) %4)
              (load (ptr %24) %6)
              (field %25 %24 0)
              (store %23 %25)
              (load (ptr %26) %6)
              (field %27 %26 1)
              (store (ptr null) %27)
              (load (ptr %28) %5)
              (field %29 %28 1)
              (store (ptr null) %29)
              (br !30)))
            (!30 (
              (load (ptr %31) %5)
              (field %32 %31 1)
              (load (ptr %33) %32)
              (store %33 %5)
              (br !13)))
            (!34 (
              (ret void))))))
    )");
    const Result<Program, Err> res2 = parseString(program2);
    TEST(!is<Err>(res2));
    TEST(pretty(getVal(res2)) == program2);

    const std::string program3 = strip(R"(
        (preamble
          (version 0.0)
          (source hand))
        (variables
          (@bar opaque))
        (functions
          (@baz void ((ptr %0) ...) opaque))
    )");
    const Result<Program, Err> res3 = parseString(program3);
    TEST(!is<Err>(res3));
    TEST(pretty(getVal(res3)) == program3);

    return true;
}

