#ifndef SVFIR_H
#define SVFIR_H

#include "Ast.h"
#include "Err.h"
#include "Result.h"
#include "Span.h"
#include "TypedProgram.h"

namespace SVFIR {

/// Parse a <program>, consuming the stream.
/// See parseFile and parseString for shortcuts.
Result<Program, Err> parse(InputStream &);

/// Check program for errors and produce queriable type information.
Result<TypedProgram, Err> typeProgram(const Program &);

/// Return a formatted string version of the given AST node.
std::string pretty(const std::variant<AnyNode, TypedProgram> &);

/// Return the Span associated with the given AST node.
Span span(const AnyNode &);

/// Parses a <program> from given file.
Result<Program, Err> parseFile(const std::string &filename);
/// Parses a <program> from given string.
Result<Program, Err> parseString(const std::string &str);

}  // namespacec SVFIR

#endif // SVFIR_H
