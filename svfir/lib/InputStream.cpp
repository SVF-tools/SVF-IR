#include <vector>
#include <istream>

#include "InputStream.h"

namespace SVFIR {
    const char InputStream::NEWLINE = '\n';

    InputStream::InputStream(std::basic_istream<char> &stream)
    : stream(stream), _cursor(Loc(1, 1)) { }

    char InputStream::peek(void) { return stream.peek(); }

    std::vector<char> InputStream::peekn(const unsigned int n) {
        std::vector<char> chars;
        // Ensure chars.data() can hold n chars. reserve doesn't guarantee that.
        chars.resize(n);
        stream.read(chars.data(), n);
        // May not have pulled n chars out.
        chars.resize(stream.gcount());
        return chars;
    }

    void InputStream::advance(const unsigned int n) {
        for (unsigned int i = 0; i < n; ++i) {
            const char c = stream.get();
            if (c == NEWLINE) {
                _cursor.setLine(_cursor.line() + 1);
                _cursor.setCol(1);
            } else {
                _cursor.setCol(_cursor.col() + 1);
            }
        }
    }

    bool InputStream::atEof(void) const {
        return stream.peek() == std::char_traits<char>::eof();
    }

    const Loc InputStream::cursor(void) const { return _cursor; }
}  // namespace SVFIR
