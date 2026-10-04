#ifndef INPUTSTREAM_H
#define INPUTSTREAM_H

#include <istream>
#include <vector>

#include "Span.h"

namespace SVFIR {
    /// Holds a reference to a basic_istream with which operations can be
    /// performed upon it whilst keeping tracking of line/column position.
    class InputStream {
        std::basic_istream<char> &stream;
        /// basic_istream keeps track of characters, not line/col, so we
        /// do it ourselves.
        Loc _cursor;

    public:
        static const char NEWLINE;

        /// Wrap stream and start the cursor at <1, 1>.
        InputStream(std::basic_istream<char> &stream);

        /// Returns the next character. Does not advance.
        /// Follows basic_istream::peak in handling EOF.
        char peek(void);

        /// Returns up to the next n characters. "Up to" because we may hit EOF
        /// for which it only returns characters before. Does not advance.
        std::vector<char> peekn(const unsigned int n);

        /// Move forward n characters or to EOF, whichever comes first.
        void advance(const unsigned int n);

        /// Indicates the stream is at EOF.
        bool atEof(void) const;

        /// Return the current position of the cursor.
        const Loc cursor(void) const;
    };

}  // namespace SVFIR

#endif // INPUTSTREAM_H
