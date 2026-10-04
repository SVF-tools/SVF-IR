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

        Span(const Loc start, const Loc end) : start(start), end(end) { }

        /// Formatted as [<line1, col1>, <line2, col2>).
        std::string toString(void) const;
    };
}  // namespace SVFIR

#endif // SPAN_H
