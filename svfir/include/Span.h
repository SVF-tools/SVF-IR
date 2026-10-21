#ifndef SPAN_H
#define SPAN_H

#include <cstdint>

namespace SVFIR {

/// Single point in a stream.
class Loc {
    uint32_t _line;
    uint32_t _col;

public:
    Loc(const uint32_t line, const uint32_t col)
    : _line(line), _col(col) { }

    uint32_t line(void) const { return _line; }
    uint32_t col(void) const { return _col; }

    void setLine(const uint32_t line) { _line = line; }
    void setCol(const uint32_t col) { _col = col; }

    /// Formatted as <line, column>.
    std::string toString(void) const;

    bool operator==(const Loc &y) const;
    bool operator!=(const Loc &y) const;
};

/// Span across lines/columns within a stream. Represents a half-open
/// interval with inclusive start and exclusive end.
struct Span {
    const Loc start;
    const Loc end;
    /// Whether this span is related to something generated out of thin air.
    /// TODO: this needs some design. E.g., sometimes we may want gen and real
    /// start/ends, sometimes without start/ends. This field cannot express
    /// that.
    const bool gen;

    Span(const Loc start, const Loc end)
    : start(start), end(end), gen(false) { }

    Span(const Loc start, const Loc end, const bool gen)
    : start(start), end(end), gen(gen) { }

    /// Formatted as [<line1, col1>, <line2, col2>).
    std::string toString(void) const;
};

/// A span to use when fabricating AST nodes without precedent.
inline const Span FAB_SPAN(Loc(0, 0), Loc(0, 0), true);

}  // namespace SVFIR

#endif // SPAN_H
