#ifndef PARSER_H
#define PARSER_H

#include "SVFIR/Ast.h"
#include "SVFIR/Err.h"
#include "SVFIR/InputStream.h"

namespace SVFIR {

/// Parse a <program>, consuming the stream.
/// See parseFile and parseString for shortcuts.
Result<Program> parse(InputStream &);

/// Parses a <program> from given file.
Result<Program> parseFile(const std::string &filename);
/// Parses a <program> from given string.
Result<Program> parseString(const std::string &str);

}  // namespacec SVFIR

#endif  // PARSER_H
