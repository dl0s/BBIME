#ifndef BBIME_TEXTPOSITIONS_H
#define BBIME_TEXTPOSITIONS_H

namespace bbime {
inline bool surrogatePair(const unsigned short *text, int length, int offset) {
    return offset + 1 < length && text[offset] >= 0xd800 && text[offset] <= 0xdbff &&
        text[offset + 1] >= 0xdc00 && text[offset + 1] <= 0xdfff;
}

inline int utf16Offset(const unsigned short *text, int length, int position,
                       bool codePoints) {
    if (position <= 0) return 0;
    if (!codePoints) return position < length ? position : length;
    int offset = 0;
    for (int i = 0; i < position && offset < length; ++i)
        offset += surrogatePair(text, length, offset) ? 2 : 1;
    return offset;
}

inline int editorOffset(const unsigned short *text, int length, int offset,
                        bool codePoints) {
    if (offset <= 0) return 0;
    if (offset > length) offset = length;
    if (!codePoints) return offset;
    int position = 0;
    for (int i = 0; i < offset; ++position)
        i += surrogatePair(text, length, i) ? 2 : 1;
    return position;
}
}
#endif
