#ifndef NP_UTF_HPP
#define NP_UTF_HPP

#include <cstddef>
#include <cassert>

namespace utf {
    struct utf8;
    struct utf16;
    struct utf32;

    typedef char32_t codepoint_type;

    bool validate_codepoint(codepoint_type c) {
        if (c < 0xd800) { return true; }
        if (c < 0xe000) { return false; }
        if (c < 0x110000) { return true; }

        return false;
    }

    template <typename E>
    struct utf_traits;

    template <>
    struct utf_traits<utf8> {
        typedef char codeunit_type;
        static size_t read_length(codeunit_type c) {
            if ((c & 0x80) == 0x00) { return 1; }
            if ((c & 0xe0) == 0xc0) { return 2; }
            if ((c & 0xf0) == 0xe0) { return 3; }
            if ((c & 0xf8) == 0xf0) { return 4; }

            return 1;
        }
        static size_t write_length(codepoint_type c) {
            if (c <= 0x7f) { return 1; }
            if (c < 0x0800) { return 2; }
            if (c < 0xd800) { return 3; }
            if (c < 0xe000) { return 0; }
            if (c < 0x010000) { return 3; }
            if (c < 0x110000) { return 4; }

            return 0;
        }

        // responsible only for validating the utf8 encoded subsequence, not the codepoint it maps to
        template <typename T>
        static bool validate(const T* first, const T* last) {
            size_t len = last - first;
            unsigned char lead = (unsigned char)*first;
            switch (len) {
                case 1:
                    if ((lead & 0x80) != 0x00) { return false; }
                    break;
                case 2:
                    if ((lead & 0xe0) != 0xc0) { return false; }
                    break;
                case 3:
                    if ((lead & 0xf0) != 0xe0) { return false; }
                    break;
                case 4:
                    if ((lead & 0xf8) != 0xf0) { return false; }
                    break;
                default:
                    return false;
            }

            for (size_t i = 1; i < len; ++i) {
                unsigned char c = first[i];
                if ((c & 0xc0) != 0x80) { return false; }
            }

            // check for overlong encodings
            switch (len) {
                case 2:
                    if (((unsigned char)*first) <= 0xc1) { return false; }
                    break;
                case 3:
                    if (((unsigned char)*first) == 0xe0) { return false; }
                    break;
                case 4:
                    if (((unsigned char)*first) == 0xf0
                        && ((unsigned char)first[1]) < 0x90) { return false; }
                    break;
                default: break;
            }

            return true;
        }

        template <typename OutIt>
        static OutIt encode(codepoint_type c, OutIt dest) {

            size_t len = write_length(c);

            unsigned char res[4] = {};

            // loop to catch remaining
            for (size_t i = len; i != 1; --i) {
                // select lower 6 bits
                res[i-1] = (c & 0x3f) | 0x80;
                c = c >> 6;
            }

            // switch on first byte
            switch (len) {
                case 1: res[0] = c; break;
                case 2: res[0] = c | 0xc0; break;
                case 3: res[0] = c | 0xe0; break;
                case 4: res[0] = c | 0xf0; break;
                default:
                    assert(false && "bad utf8 codeunit");
            };

            for (size_t i = 0; i < len; ++i) {
                *dest = res[i];
                ++dest;
            }

            return dest;
        }

        template <typename T>
        static codepoint_type decode(const T* c) {
            size_t len = read_length(*c);

            codepoint_type res = 0;
            // switch on first byte
            switch (len) {
                case 1: res = *c; break;
                case 2: res = *c & 0x1f; break;
                case 3: res = *c & 0x0f; break;
                case 4: res = *c & 0x07; break;
                default:
                    assert(false && "bad utf8 codeunit");
            };

            // then loop to catch remaining?
            for (size_t i = 1; i < len; ++i) {
                res = (res << 6) | (c[i] & 0x3f);
            }
            return res;
        }
    };

    template <>
    struct utf_traits<utf16> {
        typedef char16_t codeunit_type;
        static size_t read_length(codeunit_type c) {
            if (c < 0xd800) { return 1; }
            if (c < 0xdc00) { return 2; }
            if (c < 0x010000) { return 1; }
            return 1;
        }
        static size_t write_length(codepoint_type c) {
            if (c < 0xd800) { return 1; }
            if (c < 0xe000) { return 0; }
            if (c < 0x010000) { return 1; }
            if (c < 0x110000) { return 2; }

            return 0;
        }

        template <typename T>
        static bool validate(const T* first, const T* last) {
            size_t len = last - first;
            switch (len) {
                case 1:
                {
                    uint16_t lead = *first;
                    if (lead >= 0xd800 && lead < 0xe000) { return false; }
                    break;
                }
                case 2:
                {
                    uint16_t lead = first[0];
                    uint16_t trail = first[1];
                    if (lead < 0xd800 || lead >= 0xdc00) { return false; }
                    if (trail < 0xdc00 || trail >= 0xe000) { return false; }
                    break;
                }
                default:
                    return false;
            }
            return true;
        }
        template <typename OutIt>
        static OutIt encode(codepoint_type c, OutIt dest) {
            size_t len = write_length(c);

            if (len == 1) {
                *dest = c;
                ++dest;
                return dest;
            }

            // 20-bit intermediate value
            size_t tmp = c - 0x10000;

            *dest = static_cast<char16_t>((tmp >> 10) + 0xd800);
            ++dest;
            *dest = static_cast<char16_t>((tmp & 0x03ff) + 0xdc00);
            ++dest;
            return dest;
        }

        template <typename T>
        static codepoint_type decode(const T* c) {
            size_t len = read_length(*c);

            char16_t lead = *c;
            if (len == 1) {
                return lead;
            }

            codepoint_type res = 0;
            // 10 most significant bits
            res = (lead - 0xd800) << 10;
            uint16_t trail = c[1];
            // 10 least significant bits
            res +=  (trail - 0xdc00);
            return res + 0x10000;
        }
    };

    template <>
    struct utf_traits<utf32> {
        typedef char32_t codeunit_type;
        static size_t read_length(codeunit_type c) { return 1; }
        static size_t write_length(codepoint_type c) {
            if (c < 0xd800) { return 1; }
            if (c < 0xe000) { return 0; }
            if (c < 0x110000) { return 1; }
            return 0;
        }

        template <typename T>
        static bool validate(const T* first, const T* last) {
            // actually looking at the cp value is done by free validate function.
            return last - first == 1;
        }

        template <typename OutIt>
        static OutIt encode(codepoint_type c, OutIt dest) {
            *dest = c;
            ++dest;
            return dest;
        }
        template <typename T>
        static codepoint_type decode(const T* c) {
            return *c;
        }
    };
}
#endif
