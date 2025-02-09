/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <wtf/unicode/UTF8Conversion.h>

#include <unicode/uchar.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/StringHasherInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTF::Unicode {

static constexpr char32_t sentinelCodePoint = U_SENTINEL;

bool convertLatin1ToUTF8(std::span<const LChar> source, char** targetStart, const char* targetEnd)
{
    char* target = *targetStart;
    int32_t i = 0;
    for (auto sourceCharacter : source) {
        UBool sawError = false;
        // Work around bug in either Windows compiler or old version of ICU, where passing a uint8_t to
        // U8_APPEND warns, by converting from uint8_t to a wider type.
        char32_t character = sourceCharacter;
        U8_APPEND(target, i, targetEnd - *targetStart, character, sawError);
        ASSERT_WITH_MESSAGE(!sawError, "UTF8 destination buffer was not big enough");
        if (sawError)
            return false;
    }
    *targetStart = target + i;
    return true;
}

ConversionResult convertUTF16ToUTF8(std::span<const UChar>& sourceSpan, char** targetStart, const char* targetEnd, bool strict)
{
    auto result = ConversionResult::Success;
    auto* source = sourceSpan.data();
    auto* sourceEnd = sourceSpan.data() + sourceSpan.size();
    char* target = *targetStart;
    UBool sawError = false;
    int32_t i = 0;
    while (source < sourceEnd) {
        char32_t ch;
        int j = 0;
        U16_NEXT(source, j, sourceEnd - source, ch);
        if (U_IS_SURROGATE(ch)) {
            if (source + j == sourceEnd && U_IS_SURROGATE_LEAD(ch)) {
                result = ConversionResult::SourceExhausted;
                break;
            }
            if (strict) {
                result = ConversionResult::SourceIllegal;
                break;
            }
            ch = replacementCharacter;
        }
        U8_APPEND(reinterpret_cast<uint8_t*>(target), i, targetEnd - target, ch, sawError);
        if (sawError) {
            result = ConversionResult::TargetExhausted;
            break;
        }
        source += j;
    }
    sourceSpan = { source, sourceEnd };
    *targetStart = target + i;
    return result;
}

template<bool replaceInvalidSequences>
bool convertUTF8ToUTF16Impl(std::span<const char8_t> source, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    RELEASE_ASSERT(source.size() <= std::numeric_limits<int>::max());
    UBool error = false;
    UChar* target = *targetStart;
    size_t targetSize = targetEnd - target;
    char32_t orAllData = 0;
    size_t targetOffset = 0;
    for (size_t sourceOffset = 0; sourceOffset < source.size(); ) {
        char32_t character;
        if constexpr (replaceInvalidSequences) {
            U8_NEXT_OR_FFFD(source, sourceOffset, source.size(), character);
        } else {
            U8_NEXT(source, sourceOffset, source.size(), character);
            if (character == sentinelCodePoint)
                return false;
        }
        U16_APPEND(target, targetOffset, targetSize, character, error);
        if (error)
            return false;
        orAllData |= character;
    }
    RELEASE_ASSERT(target + targetOffset <= targetEnd);
    *targetStart = target + targetOffset;
    if (sourceAllASCII)
        *sourceAllASCII = isASCII(orAllData);
    return true;
}

bool convertUTF8ToUTF16(std::span<const char8_t> source, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    return convertUTF8ToUTF16Impl<false>(source, targetStart, targetEnd, sourceAllASCII);
}

bool convertUTF8ToUTF16ReplacingInvalidSequences(std::span<const char8_t> source, UChar** targetStart, const UChar* targetEnd, bool* sourceAllASCII)
{
    return convertUTF8ToUTF16Impl<true>(source, targetStart, targetEnd, sourceAllASCII);
}

ComputeUTFLengthsResult computeUTFLengths(std::span<const char8_t> source)
{
    size_t lengthUTF16 = 0;
    char32_t orAllData = 0;
    ConversionResult result = ConversionResult::Success;
    size_t sourceOffset = 0;
    while (sourceOffset < source.size()) {
        char32_t character;
        size_t nextSourceOffset = sourceOffset;
        U8_NEXT(source, nextSourceOffset, source.size(), character);
        if (character == sentinelCodePoint) {
            result = nextSourceOffset == source.size() ? ConversionResult::SourceExhausted : ConversionResult::SourceIllegal;
            break;
        }
        sourceOffset = nextSourceOffset;
        lengthUTF16 += U16_LENGTH(character);
        orAllData |= character;
    }
    return { result, sourceOffset, lengthUTF16, isASCII(orAllData) };
}

unsigned calculateStringHashAndLengthFromUTF8MaskingTop8Bits(std::span<const char> span, unsigned& dataLength, unsigned& utf16Length)
{
    StringHasher stringHasher;
    utf16Length = 0;
    size_t inputOffset = 0;
    auto* data = span.data();
    size_t inputLength = span.size();
    while (inputOffset < inputLength) {
        char32_t character;
        U8_NEXT(data, inputOffset, inputLength, character);
        if (character == sentinelCodePoint)
            return 0;
        if (U_IS_BMP(character)) {
            ASSERT(!U_IS_SURROGATE(character));
            stringHasher.addCharacter(character);
            utf16Length++;
        } else {
            ASSERT(U_IS_SUPPLEMENTARY(character));
            stringHasher.addCharacter(U16_LEAD(character));
            stringHasher.addCharacter(U16_TRAIL(character));
            utf16Length += 2;
        }
    }
    dataLength = inputOffset;
    return stringHasher.hashWithTop8BitsMasked();
}

bool equalUTF16WithUTF8(const UChar* a, const char* b, const char* bEnd)
{
    // It is the caller's responsibility to ensure a is long enough, which is why it is safe to use U16_NEXT_UNSAFE here.
    size_t offsetA = 0;
    size_t offsetB = 0;
    size_t lengthB = bEnd - b;
    while (offsetB < lengthB) {
        char32_t characterB;
        U8_NEXT(b, offsetB, lengthB, characterB);
        if (characterB == sentinelCodePoint)
            return false;
        char16_t characterA;
        U16_NEXT_UNSAFE(a, offsetA, characterA);
        if (characterB != characterA)
            return false;
    }
    return true;
}

bool equalLatin1WithUTF8(const LChar* a, const char* b, const char* bEnd)
{
    // It is the caller's responsibility to ensure a is long enough, which is why it is safe to use *a++ here.
    size_t offsetB = 0;
    size_t lengthB = bEnd - b;
    while (offsetB < lengthB) {
        char32_t characterB;
        U8_NEXT(b, offsetB, lengthB, characterB);
        if (*a++ != characterB)
            return false;
    }
    return true;
}

} // namespace WTF::Unicode
