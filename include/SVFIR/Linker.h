#include "SVFIR/Ast.h"
#include "SVFIR/Err.h"
#include "SVFIR/TypedProgram.h"

namespace SVFIR {

/// Used to control conflicts in linking. Described in link below.
enum class Linkage { WEAK, STRONG };

/// Links the definitions (functions, global variables) from the second
/// program tp2 into the first program tp1 producing a new program.
///
/// For every function/global variable x in tp1:
///  1) if x is a function/variable and the same name appears in tp2 as a
///  variable/function respectively, an error is produced.
///  2) if x is a function and a function of the same name appears in tp2 and
///  has a different signature, an error is produced.
///  3) if x has a body and a function/variable of the same name has a body in
///  tp2:
///    a) if under WEAK linkage, the resulting program includes x only.
///    b) if under STRONG linkage, an error is produced.
///  4) if x has a body and a function/variable of the same name is opaque in
///  tp2, the resulting program includes x.
///  5) if x is opaque and a function/variable of the same name has a
///  body in tp2, the resulting program includes the function/variable from
///  tp2.
///  6) if x is opaque in tp1 and a function/variable of the same name is opaque
///  in tp2, the resulting program includes x.
Result<TypedProgram> link(
    const TypedProgram &,
    const TypedProgram &,
    const Linkage
);

}  // namespace SVFIR
