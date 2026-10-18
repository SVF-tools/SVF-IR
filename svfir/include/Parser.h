#include <variant>

#include "Ast.h"
#include "InputStream.h"
#include "Result.h"
#include "SExprs.h"

namespace SVFIR {

/// Parse a <program>, consuming the stream.
Result<Program, Err> parse(InputStream &);

/// Parses a <program> from given file.
Result<Program, Err> parseFile(const std::string &filename);
/// Parses a <program> from given string.
Result<Program, Err> parseString(const std::string &str);

}  // namespace SVFIR
