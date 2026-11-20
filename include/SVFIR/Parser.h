#ifndef PARSER_H
#define PARSER_H

namespace SVFIR {

/// Parse a <program>, consuming the stream.
/// See parseFile and parseString for shortcuts.
Result<Program, Err> parse(InputStream &);

/// Parses a <program> from given file.
Result<Program, Err> parseFile(const std::string &filename);
/// Parses a <program> from given string.
Result<Program, Err> parseString(const std::string &str);

}  // namespacec SVFIR

#endif  // PARSER_H
