﻿/*
* (c) Проект "Core.As", Александр Орефков orefkov@gmail.com
* Реализация строковых функций
*/
#include "core_as/str/simple_unicode.h"
#include "core_as/str/sstring.h"

#ifdef _WIN32
extern "C" {
int __stdcall MultiByteToWideChar(
    unsigned CodePage, unsigned dwFlags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);

int __stdcall WideCharToMultiByte(
    unsigned CodePage,
    unsigned dwFlags,
    const wchar_t* lpWideCharStr,
    int cchWideChar,
    char* lpMultiByteStr,
    int cbMultiByte,
    const char* lpDefaultChar,
    bool* lpUsedDefaultChar);
}

#define CP_UTF8 65001 // UTF-8 translation

#endif

namespace core_as::str {

template class sstring<u8symbol>;
template class sstring<uwsymbol>;
template class sstring<u16symbol>;
template class sstring<u32symbol>;

// from sqlite.c
/*
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts over-length UTF8 encodings
**     for unicode values 0x80 and greater.  It does not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
static const uu8symbol sqlite3Utf8Trans1[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

u32symbol readUtf8Symbol(const uu8symbol*& ptr, const uu8symbol* end) {
    u32symbol us = static_cast<u32symbol>(*ptr++);
    if (us >= 0xC0) {
        us = sqlite3Utf8Trans1[us - 0xC0];
        while (ptr != end && (*ptr & 0xC0) == 0x80)
            us = (us << 6) + (0x3F & *(ptr++));
        if (us < 0x80 || (us & 0xFFFFF800) == 0xD800 || (us & 0xFFFFFFFE) == 0xFFFE) {
            us = 0xFFFD;
        }
    }
    return us;
}

void writeUtf8Symbol(uu8symbol*& write, u32symbol us) {
    using u8 = uu8symbol;
    if (us <= 0x80)
        *write++ = (u8)us;
    else if (us < 0x00800) {
        *write++ = 0xC0 + (u8)((us >> 6) & 0x1F);
        *write++ = 0x80 + (u8)(us & 0x3F);
    } else if (us < 0x10000) {
        *write++ = 0xE0 + (u8)((us >> 12) & 0x0F);
        *write++ = 0x80 + (u8)((us >> 6) & 0x3F);
        *write++ = 0x80 + (u8)(us & 0x3F);
    } else {
        *write++ = 0xF0 + (u8)((us >> 18) & 0x07);
        *write++ = 0x80 + (u8)((us >> 12) & 0x3F);
        *write++ = 0x80 + (u8)((us >> 6) & 0x3F);
        *write++ = 0x80 + (u8)(us & 0x3F);
    }
}

u32symbol readUtf16Symbol(const u16symbol*& ptr, const u16symbol* end) {
    u32symbol us = static_cast<u32symbol>(*ptr++);
    if (us >= 0xD800) {
        if (us < 0xDC00 && ptr < end) {
            if (u32symbol s = static_cast<u32symbol>(*ptr++); s >= 0xDC00 && s < 0xE000) {
                return ((us & 0x3ff) << 10) + (s & 0x3ff) + 0x10000;
            }
        }
        return 0xFFFD;
    }
    return us;
}

void writeUtf16Symbol(u16symbol*& ptr, u32symbol s) {
    if (s < 0x10000) {
        *ptr++ = static_cast<u16symbol>(s);
    } else {
        s -= 0x10000;
        *ptr++ = 0xD800 + ((s >> 10) & 0x3FF);
        *ptr++ = 0xDC00 + (s & 0x3FF);
    }
}

size_t utf8len(u32symbol us) {
    if (us <= 0x80)
        return 1;
    else if (us < 0x00800)
        return 2;
    else if (us < 0x10000)
        return 3;
    else
        return 4;
}

COREAS_API size_t utf_convert_selector<u8symbol, u16symbol>::convert(const u8symbol* src, size_t srcLen, u16symbol* dest) {
#ifndef _WIN32
    const u8symbol* end = src + srcLen;
    u16symbol* ptr      = dest;
    while (src < end)
        writeUtf16Symbol(ptr, readUtf8Symbol((const uu8symbol*&)src, (const uu8symbol*)end));
    return static_cast<size_t>(ptr - dest);
#else
    return MultiByteToWideChar(CP_UTF8, 0, src, static_cast<unsigned>(srcLen), (wchar_t*)dest, static_cast<unsigned>(srcLen) * 8);
#endif
}

COREAS_API size_t utf_convert_selector<u8symbol, u32symbol>::convert(const u8symbol* src, size_t srcLen, u32symbol* dest) {
    const u8symbol* end = src + srcLen;
    u32symbol* ptr      = dest;
    while (src < end)
        *ptr++ = readUtf8Symbol((const uu8symbol*&)src, (const uu8symbol*)end);
    return static_cast<size_t>(ptr - dest);
}

COREAS_API size_t utf_convert_selector<u16symbol, u8symbol>::convert(const u16symbol* src, size_t srcLen, u8symbol* dest) {
#ifndef _WIN32
    const u16symbol* end = src + srcLen;
    u8symbol* ptr        = dest;
    while (src < end)
        writeUtf8Symbol((uu8symbol*&)ptr, readUtf16Symbol(src, end));
    return static_cast<size_t>(ptr - dest);
#else
    return WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)src, static_cast<unsigned>(srcLen), dest, static_cast<unsigned>(srcLen) * 8, "?", nullptr);
#endif
}

COREAS_API size_t utf_convert_selector<u16symbol, u32symbol>::convert(const u16symbol* src, size_t srcLen, u32symbol* dest) {
    const u16symbol* end = src + srcLen;
    u32symbol* ptr       = dest;
    while (src < end)
        *ptr++ = readUtf16Symbol(src, end);
    return static_cast<size_t>(ptr - dest);
}

COREAS_API size_t utf_convert_selector<u32symbol, u8symbol>::convert(const u32symbol* src, size_t srcLen, u8symbol* dest) {
    u8symbol* ptr = dest;
    for (size_t i = 0; i < srcLen; i++)
        writeUtf8Symbol((uu8symbol*&)ptr, *src++);
    return static_cast<size_t>(ptr - dest);
}

COREAS_API size_t utf_convert_selector<u32symbol, u16symbol>::convert(const u32symbol* src, size_t srcLen, u16symbol* dest) {
    u16symbol* ptr = dest;
    for (size_t i = 0; i < srcLen; i++)
        writeUtf16Symbol(ptr, *src++);
    return static_cast<size_t>(ptr - dest);
}

inline u32symbol makeLowerU(u32symbol s) {
    if (isAsciiUpper(s))
        return s | 0x20;
    else if (s > 127 && s < 0x10000)
        return simpleUnicodeLower(static_cast<uint16_t>(s));
    else
        return s;
}

inline u32symbol makeUpperU(u32symbol s) {
    if (isAsciiLower(s))
        return s & ~0x20;
    else if (s > 127 && s < 0x10000)
        return simpleUnicodeUpper(static_cast<uint16_t>(s));
    else
        return s;
}

inline u32symbol makeFoldU(u32symbol s) {
    if (isAsciiUpper(s))
        return s | 0x20;
    else if (s > 127 && s < 0x10000)
        return simpleUnicodeFold(static_cast<uint16_t>(s));
    else
        return s;
}

/*
* Поиск utf-8 symbols, которые меняют свою длину при upper/lower преобразовании
* вот они нашлись
* "İiıIſSȺⱥȾⱦɐⱯɑⱭɫⱢɱⱮɽⱤẞßιΙΩωKkÅåⱢɫⱤɽⱥȺⱦȾⱭɑⱮɱⱯɐ"
   l2u ıIſSɐⱯɑⱭɫⱢɱⱮɽⱤιΙⱥȺⱦȾ
   u2l İiȺⱥȾⱦẞßΩωKkÅåⱢɫⱤɽⱭɑⱮɱⱯɐ
void findStrange() {
    u16symbol badsL2U[100], * pBadslu = badsL2U;
    u16symbol badsU2L[100], * pBadsul = badsU2L;
    uu8symbol buf[20];
    size_t allToUpper = 0, allToLower = 0, lowerbadshorter = 0, upperbadshorter = 0, lowerbadlonger = 0, upperbadlonger = 0;
    for (u32symbol s = 128; s <= 0xFFFF; s++) {
        u16symbol upper = makeUpperU(s);
        if (upper != s) {
            allToUpper++;
            uu8symbol* pTest = buf;
            writeUtf8Symbol(pTest, s);
            size_t len1 = pTest - buf;
            pTest = buf;
            writeUtf8Symbol(pTest, upper);
            size_t len2 = pTest - buf;
            if (len1 != len2) {
                *pBadslu++ = s;
                *pBadslu++ = upper;
                if (len1 > len2) {
                    upperbadshorter++;
                } else if (len1 < len2) {
                    upperbadlonger++;
                }
            }
        }
        u16symbol lower = makeLowerU(s);
        if (lower != s) {
            allToLower++;
            uu8symbol* pTest = buf;
            writeUtf8Symbol(pTest, s);
            size_t len1 = pTest - buf;
            pTest = buf;
            writeUtf8Symbol(pTest, lower);
            size_t len2 = pTest - buf;
            if (len1 != len2) {
                *pBadsul++ = s;
                *pBadsul++ = lower;
                if (len1 > len2) {
                    lowerbadshorter++;
                } else if (len1 < len2) {
                    lowerbadlonger++;
                }
            }
        }
    }
    *pBadsul = 0;
    *pBadslu = 0;
    return;
}
*/

template<typename Op> size_t utf8_case_change(const u8symbol*& src, size_t len, u8symbol*& dest, size_t lenBuffer, const Op& op) {
    // Допущение для оптимизации - считается, что выделенный буфер записи всегда не меньше длины буфера чтения
    // и что буфер записи не начинается внутри буфера чтения
    // то есть если символ считан, и длина записываемого символа не больше считанного, то он поместится в буфер записи
    // и не перетрет символы, которые еще не прочитали
    // По другому работать откажемся
    if (lenBuffer < len || (dest > src && dest < src + len))
        return len;
    const uu8symbol *beginReadPos = reinterpret_cast<const uu8symbol*>(src), *readPos = beginReadPos, *endReadPos = beginReadPos + len,
                    *readFromPos = readPos;
    uu8symbol *beginWritePos = reinterpret_cast<uu8symbol*>(dest), *writePos = beginWritePos, *endWritePos = writePos + lenBuffer, *tempWrite;

    size_t state = 0, writedSymbolLen, needExtraLen = 0;
    u32symbol readedSymbol, writedSymbol;
    lstringa<4096> tempStore;

    while (readPos < endReadPos) {
        readedSymbol = readUtf8Symbol(readPos, endReadPos), writedSymbol = op(readedSymbol);
        writedSymbolLen = utf8len(writedSymbol);
        switch (state) {
        case 0: // начальное состояние
            // Так как выделенный буфер записи всегда не меньше длины буфера чтения
            // то если символ считан, и длина записываемого символа не больше считанного,
            // то он поместится в буфер записи, можно не проверять
            // оптимизация для inplace конвертации в идеальном случае
            if (writePos == readFromPos && readedSymbol == writedSymbol) {
                writePos    = const_cast<uu8symbol*>(readPos);
                readFromPos = readPos;
                continue;
            }
            if (writePos + writedSymbolLen <= readPos) {
                // всё отлично
                writeUtf8Symbol(writePos, writedSymbol);
            } else {
                state = 1;
                goto state1;
            }
            break;
        case 1:
        state1:
            // До этого когда-то был считан символ, при записи ставший длиннее.
            // Можно его писать, но надо проверять, что записываемый символ:
            // - не перезатрёт читаемые
            // - не выйдет за границы буфера записи
            // - восстановит обычный режим, если записываемый символ короче прочитанного
            if (writePos + writedSymbolLen > endWritePos) {
                // Записываемый символ не поместится в буфер записи
                // Запомним, где это произошло, подсчитаем символы далее
                src          = reinterpret_cast<const u8symbol*>(readFromPos);
                dest         = reinterpret_cast<u8symbol*>(writePos);
                needExtraLen = 0;
                state        = 2;
                goto state2;
            } else if (writePos < readPos && writePos + writedSymbolLen > readPos) {
                // Совсем плохой случай - записываемый символ перезатрет следующий читаемый
                // Запомним, где это произошло, подсчитаем символы далее
                src          = reinterpret_cast<const u8symbol*>(readFromPos);
                dest         = reinterpret_cast<u8symbol*>(writePos);
                needExtraLen = 0;
                if (lenBuffer > len) {
                    // считаем, что это второй вызов, и места в буфере в итоге хватит
                    // поэтому будем сохранять считанные символы в свой буфер, а потом скопируем их в результат
                    tempWrite = reinterpret_cast<uu8symbol*>(tempStore.reserve(lenBuffer - static_cast<size_t>(writePos - beginWritePos)));
                    state     = 3;
                    goto state3;
                } else {
                    // Это первый вызов, будем просто считать нужное место
                    state = 2;
                    goto state2;
                }
            } else {
                // Можем записать прочитанный символ
                writeUtf8Symbol(writePos, writedSymbol);
                // Возможно, мы выровнялись по прочитанным/записанным байтам, тогда можно снова писать в буфер
                // в обычном состоянии
                size_t allReaded = static_cast<size_t>(readPos - beginReadPos);
                size_t allWrite  = static_cast<size_t>(writePos - beginWritePos);
                if (allReaded >= allWrite) {
                    // вернулись в обычное состояние
                    state = 0;
                }
            }
            break;
        case 2:
        state2:
            // Место в буфере кончилось, просто считаем нужное место
            needExtraLen += writedSymbolLen;
            break;
        case 3:
        state3:
            // Был прочитан удлинившийся символ, который мог перезатереть читаемые.
            // Конец буфера записи не достигнут, но писать туда нельзя.
            // При этом известно, что место в буфере выделено
            // с нужным запасом, и сейчас мы просто запоминаем считанные символы, чтобы
            // потом вписать их когда позволит указатель чтения
            if (writePos + writedSymbolLen <= readPos || readPos == endReadPos) {
                // Опа, символ стал короче и снова влезает в буфер. Или все прочитали, можно записывать
                std::char_traits<u8symbol>::copy(dest, tempStore.symbols(), needExtraLen);
                writeUtf8Symbol(writePos, writedSymbol);
                state = 0;
            } else {
                if (writePos + writedSymbolLen > endWritePos) {
                    // Таки всё-равно не влезем в буфер
                    state = 2;
                    goto state2;
                }
                writeUtf8Symbol(tempWrite, writedSymbol);
                writePos += writedSymbolLen;
                needExtraLen += writedSymbolLen;
            }
            break;
        }
        readFromPos = readPos;
    }
    if (state == 0 || state == 1) {
        // все отлично, можно возвращать получившуюся длину
        src  = reinterpret_cast<const u8symbol*>(readFromPos);
        dest = reinterpret_cast<u8symbol*>(writePos);
        return static_cast<size_t>(writePos - beginWritePos);
    } else {
        size_t writedCount = static_cast<size_t>(dest - (u8symbol*)beginWritePos);
        size_t needBuffer  = writedCount + needExtraLen;
        if (needBuffer <= lenBuffer) {
            // значит попали во 2ое состояние и считали символы, так как перезатирали читаемые
            // но видимо потом попались укоротившиеся и по итогу всё-равно все влезет, только
            // нужно временно сохранять символы. Запустим еще раз
            size_t readedCount = static_cast<size_t>((uu8symbol*)src - beginReadPos);
            len -= readedCount;
            lenBuffer -= writedCount; // по идее они должны быть одинаковые
            utf8_case_change(src, len, dest, lenBuffer + 1, op);
        }
        // в dest и src уже сохранены последние позиции. Вернем нужную длину
        return needBuffer;
    }
}

COREAS_API size_t unicode_traits<u8symbol>::upper(const u8symbol*& src, size_t len, u8symbol*& dest, size_t lenBuffer) {
    return utf8_case_change(src, len, dest, lenBuffer, makeUpperU);
}

COREAS_API size_t unicode_traits<u8symbol>::lower(const u8symbol*& src, size_t len, u8symbol*& dest, size_t lenBuffer) {
    return utf8_case_change(src, len, dest, lenBuffer, makeLowerU);
}

template<typename Op> size_t utf8_findFirstCase(const u8symbol* src, size_t len, const Op& op) {
    const uu8symbol *beginReadPos = reinterpret_cast<const uu8symbol*>(src), *readPos = beginReadPos, *endReadPos = beginReadPos + len,
                    *pFrom;
    while (readPos < endReadPos) {
        pFrom        = readPos;
        u32symbol s1 = readUtf8Symbol(readPos, endReadPos), s2 = op(s1);
        if (s1 != s2)
            return static_cast<size_t>(pFrom - beginReadPos);
    }
    return str_pos::badIdx;
}

COREAS_API size_t unicode_traits<u8symbol>::findFirstUpper(const u8symbol* src, size_t len) {
    return utf8_findFirstCase(src, len, makeLowerU);
}

COREAS_API size_t unicode_traits<u8symbol>::findFirstLower(const u8symbol* src, size_t len) {
    return utf8_findFirstCase(src, len, makeUpperU);
}

template<typename Op> void utf16_change_case(const u16symbol* src, size_t len, u16symbol* dest, const Op& op) {
    if (dest != src) {
        // не inplace - надо писать все символы
        for (size_t l = 0; l < len; l++)
            dest[l] = static_cast<u16symbol>(op(src[l]));
    } else {
        // inplace - будем писать только измененные символы
        for (size_t l = 0; l < len; l++) {
            u16symbol s = src[l], s1 = static_cast<u16symbol>(op(s));
            if (s != s1)
                dest[l] = s1;
        }
    }
}

COREAS_API void unicode_traits<u16symbol>::upper(const u16symbol* src, size_t len, u16symbol* dest) {
    utf16_change_case(src, len, dest, makeUpperU);
}

COREAS_API void unicode_traits<u16symbol>::lower(const u16symbol* src, size_t len, u16symbol* dest) {
    utf16_change_case(src, len, dest, makeLowerU);
}

template<typename Op> size_t utf16_findFirstCase(const u16symbol* src, size_t len, const Op& opConvert) {
    const u16symbol *readPos = src, *endReadPos = src + len, *pFrom;
    while (readPos < endReadPos) {
        pFrom        = readPos;
        u32symbol s1 = readUtf16Symbol(readPos, endReadPos), s2 = opConvert(s1);
        if (s1 != s2)
            return static_cast<size_t>(pFrom - src);
    }
    return str_pos::badIdx;
}

COREAS_API size_t unicode_traits<u16symbol>::findFirstUpper(const u16symbol* src, size_t len) {
    return utf16_findFirstCase(src, len, makeLowerU);
}

COREAS_API size_t unicode_traits<u16symbol>::findFirstLower(const u16symbol* src, size_t len) {
    return utf16_findFirstCase(src, len, makeUpperU);
}

template<typename Op> void utf32_change_case(const u32symbol* src, size_t len, u32symbol* dest, const Op& op) {
    if (dest != src) {
        // не inplace - надо писать все символы
        for (size_t l = 0; l < len; l++)
            *dest++ = op(*src++);
    } else {
        // inplace - будем писать только измененные символы
        for (size_t l = 0; l < len; l++, src++, dest++) {
            u32symbol s = *src, s1 = op(s);
            if (s != s1)
                *dest = s1;
        }
    }
}

COREAS_API void unicode_traits<u32symbol>::upper(const u32symbol* src, size_t len, u32symbol* dest) {
    utf32_change_case(src, len, dest, makeUpperU);
}

COREAS_API void unicode_traits<u32symbol>::lower(const u32symbol* src, size_t len, u32symbol* dest) {
    utf32_change_case(src, len, dest, makeLowerU);
}

template<typename Op> size_t utf32_findFirstCase(const u32symbol* src, size_t len, const Op& op) {
    const u32symbol *readPos = src, *endReadPos = src + len;
    while (readPos < endReadPos) {
        u32symbol s1 = *readPos, s2 = op(s1);
        if (s1 != s2)
            return static_cast<size_t>(readPos - src);
        readPos++;
    }
    return str_pos::badIdx;
}

COREAS_API size_t unicode_traits<u32symbol>::findFirstUpper(const u32symbol* src, size_t len) {
    return utf32_findFirstCase(src, len, makeLowerU);
}

COREAS_API size_t unicode_traits<u32symbol>::findFirstLower(const u32symbol* src, size_t len) {
    return utf32_findFirstCase(src, len, makeUpperU);
}

COREAS_API int unicode_traits<u8symbol>::compareiu(const u8symbol* text1, size_t len1, const u8symbol* text2, size_t len2) {
    if (!len1) {
        return len2 == 0 ? 0 : -1;
    } else if (!len2)
        return 1;
    const uu8symbol *ptr1 = reinterpret_cast<const uu8symbol*>(text1), *ptr2 = reinterpret_cast<const uu8symbol*>(text2),
                    *ptr1End = ptr1 + len1, *ptr2End = ptr2 + len2;

    for (;;) {
        u32symbol s1 = makeFoldU(readUtf8Symbol(ptr1, ptr1End)), s2 = makeFoldU(readUtf8Symbol(ptr2, ptr2End));
        if (s1 > s2)
            return 1;
        else if (s1 < s2)
            return -1;
        else if (ptr1 >= ptr1End)
            return ptr2 >= ptr2End ? 0 : -1;
        else if (ptr2 >= ptr2End)
            return 1;
    }
}

COREAS_API int unicode_traits<u16symbol>::compareiu(const u16symbol* text1, size_t len1, const u16symbol* text2, size_t len2) {
    if (!len1) {
        return len2 == 0 ? 0 : -1;
    } else if (!len2)
        return 1;
    const u16symbol *ptr1End = text1 + len1, *ptr2End = text2 + len2;

    for (;;) {
        u32symbol s1 = makeFoldU(readUtf16Symbol(text1, ptr1End)), s2 = makeFoldU(readUtf16Symbol(text2, ptr2End));
        if (s1 > s2)
            return 1;
        else if (s1 < s2)
            return -1;
        else if (text1 > ptr1End)
            return text2 > ptr2End ? 0 : -1;
        else if (text2 > ptr2End)
            return 1;
    }
}

COREAS_API int unicode_traits<u32symbol>::compareiu(const u32symbol* text1, size_t len1, const u32symbol* text2, size_t len2) {
    if (!len1) {
        return len2 == 0 ? 0 : -1;
    } else if (!len2)
        return 1;
    const u32symbol *ptr1End = text1 + len1, *ptr2End = text2 + len2;

    for (;;) {
        u32symbol s1 = makeFoldU(*text1++), s2 = makeFoldU(*text2++);
        if (s1 > s2)
            return 1;
        else if (s1 < s2)
            return -1;
        else if (text1 > ptr1End)
            return text2 >= ptr2End ? 0 : -1;
        else if (text2 > ptr2End)
            return 1;
    }
}

COREAS_API size_t unicode_traits<u8symbol>::hashia(const u8symbol* src, size_t l) {
    l        = std::min(l, maxLenForHash);
    size_t h = fnv::basis;
    for (size_t i = 0; i < l; i++)
        h = (h ^ (uu8symbol)makeAsciiLower(src[i])) * fnv::prime;
    return h;
}

COREAS_API size_t unicode_traits<u8symbol>::hashiu(const u8symbol* src, size_t l) {
    size_t h             = fnv::basis;
    const uu8symbol *ptr = (const uu8symbol*)src, *pEnd = ptr + l;
    l = 0;
    while (ptr < pEnd && l++ < maxLenForHash)
        h = (h ^ makeFoldU(readUtf8Symbol(ptr, pEnd))) * fnv::prime;
    return h;
}

COREAS_API size_t unicode_traits<u16symbol>::hashia(const u16symbol* src, size_t l) {
    l        = std::min(l, maxLenForHash);
    size_t h = fnv::basis;
    for (size_t i = 0; i < l; i++)
        h = (h ^ makeAsciiLower(src[i])) * fnv::prime;
    return h;
}

COREAS_API size_t unicode_traits<u16symbol>::hashiu(const u16symbol* src, size_t l) {
    size_t h              = fnv::basis;
    const u16symbol* pEnd = src + l;
    l                     = 0;
    while (src < pEnd && l++ < maxLenForHash)
        h = (h ^ makeFoldU(readUtf16Symbol(src, pEnd))) * fnv::prime;
    return h;
}

COREAS_API size_t unicode_traits<u32symbol>::hashia(const u32symbol* src, size_t l) {
    l        = std::min(l, maxLenForHash);
    size_t h = fnv::basis;
    for (size_t i = 0; i < l; i++) {
        u32symbol s = src[i];
        if (s >= 'A' && s <= 'Z')
            s |= 0x20;
        h = (h ^ s) * fnv::prime;
    }
    return h;
}

COREAS_API size_t unicode_traits<u32symbol>::hashiu(const u32symbol* src, size_t l) {
    l        = std::min(l, maxLenForHash);
    size_t h = fnv::basis;
    for (size_t i = 0; i < l; i++) {
        u32symbol s = makeFoldU(src[i]);
        h           = (h ^ s) * fnv::prime;
    }
    return h;
}

} // namespace core_as::str
