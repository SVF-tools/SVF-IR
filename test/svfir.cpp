#include "Err.h"
#include "Parser.h"
#include "Ops.h"
#include "Result.h"
#include "Variant.h"

#include "ControlFlowGraph.h"
#include "Dominator.h"

#include "test.h"

using namespace SVFIR;

bool roundtrip(void) {
    const std::string program1 =
        "(preamble\n"
        "  (version 0.0)\n"
        "  (source hand))\n"
        "(types)\n"
        "(variables)\n"
        "(functions)";
    const Result<Program, Err> res1 = parseString(program1);
    TEST(!is<Err>(res1));
    TEST(pretty(getVal(res1)) == program1);

    const std::string program2 =
        "(preamble\n"
        "  (version 0.0)\n"
        "  (source hand))\n"
        "(types\n"
        "  (~struct.node (agg 2)))\n"
        "(variables\n"
        "  (@MAX_LENGTH (i32 10)))\n"
        "(functions\n"
        "  (@count i32 ((ptr %0)) (\n"
        "    (!1 (\n"
        "      (alloc stack %2 (md (type ptr)))\n"
        "      (alloc stack %3 (md (type i32)))\n"
        "      (alloc stack %4 (md (type ptr)))\n"
        "      (store %0 %2)\n"
        "      (store (i32 0) %3)\n"
        "      (load (ptr %5) %2)\n"
        "      (store %5 %4)\n"
        "      (br !2)))\n"
        "    (!2 (\n"
        "      (load (ptr %7) %4)\n"
        "      (cmp != %8 %7 (ptr null))\n"
        "      (brif %8 !3 !4)))\n"
        "    (!3 (\n"
        "      (load (i32 %10) %3)\n"
        "      (add %11 %10 (i32 1))\n"
        "      (store %11 %3)\n"
        "      (load (ptr %12) %4)\n"
        "      (field %13 %12 1)\n"
        "      (load (ptr %14) %13)\n"
        "      (store %14 %4)\n"
        "      (br !2)))\n"
        "    (!4 (\n"
        "      (load (i32 %16) %3)\n"
        "      (ret %16)))))\n"
        "  (@append void ((ptr %0) (i32 %1)) (\n"
        "    (!1 (\n"
        "      (alloc stack %3 (md (type ptr)))\n"
        "      (alloc stack %4 (md (type i32)))\n"
        "      (alloc stack %5 (md (type ptr)))\n"
        "      (alloc stack %6 (md (type ptr)))\n"
        "      (store %0 %3)\n"
        "      (store %1 %4)\n"
        "      (load (ptr %7) %3)\n"
        "      (call %8 @count (%7))\n"
        "      (cmp = %9 %8 (i32 10))\n"
        "      (brif %9 !10 !11)))\n"
        "    (!10 (\n"
        "      (br !34)))\n"
        "    (!11 (\n"
        "      (load (ptr %12) %3)\n"
        "      (store %12 %5)\n"
        "      (br !13)))\n"
        "    (!13 (\n"
        "      (load (ptr %14) %5)\n"
        "      (cmp != %15 %14 (ptr null))\n"
        "      (brif %15 !16 !34)))\n"
        "    (!16 (\n"
        "      (load (ptr %17) %5)\n"
        "      (field %18 %17 1)\n"
        "      (load (ptr %19) %18)\n"
        "      (cmp = %20 %19 (ptr null))\n"
        "      (brif %20 !21 !30)))\n"
        "    (!21 (\n"
        "      (alloc heap %22 (md (size 16)))\n"
        "      (store %22 %7)\n"
        "      (load (i32 %23) %4)\n"
        "      (load (ptr %24) %6)\n"
        "      (field %25 %24 0)\n"
        "      (store %23 %25)\n"
        "      (load (ptr %26) %6)\n"
        "      (field %27 %26 1)\n"
        "      (store (ptr null) %27)\n"
        "      (load (ptr %28) %5)\n"
        "      (field %29 %28 1)\n"
        "      (store (ptr null) %29)\n"
        "      (br !30)))\n"
        "    (!30 (\n"
        "      (load (ptr %31) %5)\n"
        "      (field %32 %31 1)\n"
        "      (load (ptr %33) %32)\n"
        "      (store %33 %5)\n"
        "      (br !13)))\n"
        "    (!34 (\n"
        "      (ret void))))))";
    const Result<Program, Err> res2 = parseString(program2);
    TEST(!is<Err>(res2));
    TEST(pretty(getVal(res2)) == program2);

    const std::string program3 =
        "(preamble\n"
        "  (version 0.0)\n"
        "  (source hand))\n"
        "(types\n"
        "  (~foo opaque))\n"
        "(variables\n"
        "  (@bar opaque))\n"
        "(functions\n"
        "  (@baz void ((ptr %0) ...) opaque))";
    const Result<Program, Err> res3 = parseString(program3);
    TEST(!is<Err>(res3));
    TEST(pretty(getVal(res3)) == program3);

    return true;
}

int main(void) {
    bool success = true;
    success &= roundtrip();

    if (success) { std::cout << "All tests passed." << std::endl; }
    else { std::cout << "Some tests failed." << std::endl; }

    return success ? 1 : 0;
}
