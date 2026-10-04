#include "Parser.h"
#include "Pretty.h"
#include "Result.h"

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
    const Result<Program, ErrMsg> res1 = parseString(program1);
    TEST(!isErr(res1));
    TEST(pretty(getVal(res1)) == program1);

    const std::string program2 =
        "(preamble\n"
        "  (version 0.0)\n"
        "  (source hand))\n"
        "(types\n"
        "  (~struct.node (agg 2)))\n"
        "(variables\n"
        "  (@MAX_LENGTH (10 i32)))\n"
        "(functions\n"
        "  (@count ((%0 ptr)) i32 (\n"
        "    (!1 (\n"
        "      (alloc stack %2 (md (type ptr)))\n"
        "      (alloc stack %3 (md (type i32)))\n"
        "      (alloc stack %4 (md (type ptr)))\n"
        "      (store %0 %2)\n"
        "      (store (0 i32) %3)\n"
        "      (load (%5 ptr) %2)\n"
        "      (store %5 %4)\n"
        "      (br !2)))\n"
        "    (!2 (\n"
        "      (load (%7 ptr) %4)\n"
        "      (cmp %8 != %7 (null ptr))\n"
        "      (brif %8 !3 !4)))\n"
        "    (!3 (\n"
        "      (load (%10 i32) %3)\n"
        "      (add %11 %10 (1 i32))\n"
        "      (store %11 %3)\n"
        "      (load (%12 ptr) %4)\n"
        "      (field %13 %12 1)\n"
        "      (load (%14 ptr) %13)\n"
        "      (store %14 %4)\n"
        "      (br !2)))\n"
        "    (!4 (\n"
        "      (load (%16 i32) %3)\n"
        "      (ret %16)))))\n"
        "  (@append ((%0 ptr) (%1 i32)) void (\n"
        "    (!1 (\n"
        "      (alloc stack %3 (md (type ptr)))\n"
        "      (alloc stack %4 (md (type i32)))\n"
        "      (alloc stack %5 (md (type ptr)))\n"
        "      (alloc stack %6 (md (type ptr)))\n"
        "      (store %0 %3)\n"
        "      (store %1 %4)\n"
        "      (load (%7 ptr) %3)\n"
        "      (call %8 @count (%7))\n"
        "      (cmp %9 = %8 (10 i32))\n"
        "      (brif %9 !10 !11)))\n"
        "    (!10 (\n"
        "      (br !34)))\n"
        "    (!11 (\n"
        "      (load (%12 ptr) %3)\n"
        "      (store %12 %5)\n"
        "      (br !13)))\n"
        "    (!13 (\n"
        "      (load (%14 ptr) %5)\n"
        "      (cmp %15 != %14 (null ptr))\n"
        "      (brif %15 !16 !34)))\n"
        "    (!16 (\n"
        "      (load (%17 ptr) %5)\n"
        "      (field %18 %17 1)\n"
        "      (load (%19 ptr) %18)\n"
        "      (cmp %20 = %19 (null ptr))\n"
        "      (brif %20 !21 !30)))\n"
        "    (!21 (\n"
        "      (alloc heap %22 (md (size 16)))\n"
        "      (store %22 %7)\n"
        "      (load (%23 i32) %4)\n"
        "      (load (%24 ptr) %6)\n"
        "      (field %25 %24 0)\n"
        "      (store %23 %25)\n"
        "      (load (%26 ptr) %6)\n"
        "      (field %27 %26 1)\n"
        "      (store (null ptr) %27)\n"
        "      (load (%28 ptr) %5)\n"
        "      (field %29 %28 1)\n"
        "      (store (null ptr) %29)\n"
        "      (br !30)))\n"
        "    (!30 (\n"
        "      (load (%31 ptr) %5)\n"
        "      (field %32 %31 1)\n"
        "      (load (%33 ptr) %32)\n"
        "      (store %33 %5)\n"
        "      (br !13)))\n"
        "    (!30 (\n"
        "      (ret))))))";
    const Result<Program, ErrMsg> res2 = parseString(program2);
    TEST(!isErr(res2));
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
        "  (@baz ((%0 ptr) %1...) void opaque))";
    const Result<Program, ErrMsg> res3 = parseString(program3);
    TEST(!isErr(res3));
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
