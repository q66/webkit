/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSStringRef.h"
#include "JSStringRefPrivate.h"

#include "InitializeThreading.h"
#include "OpaqueJSString.h"
#include <wtf/unicode/UTF8Conversion.h>

using namespace JSC;
using namespace WTF::Unicode;

JSStringRef JSStringCreateWithCharacters(const JSChar* chars, size_t numChars)
{
    JSC::initialize();
    return &OpaqueJSString::create({ reinterpret_cast<const UChar*>(chars), numChars }).leakRef();
}

JSStringRef JSStringCreateWithUTF8CString(const char* string)
{
    JSC::initialize();
    if (string) {
        auto stringSpan = span8(string);
        Vector<UChar, 1024> buffer(stringSpan.size());
        UChar* p = buffer.data();
        bool sourceContainsOnlyASCII;
        if (convertUTF8ToUTF16(spanReinterpretCast<const char8_t>(stringSpan), &p, p + buffer.size(), &sourceContainsOnlyASCII)) {
            if (sourceContainsOnlyASCII)
                return &OpaqueJSString::create(stringSpan).leakRef();
            return &OpaqueJSString::create({ buffer.data(), p }).leakRef();
        }
    }

    return &OpaqueJSString::create().leakRef();
}

JSStringRef JSStringCreateWithCharactersNoCopy(const JSChar* chars, size_t numChars)
{
    JSC::initialize();
    return OpaqueJSString::tryCreate(StringImpl::createWithoutCopying({ reinterpret_cast<const UChar*>(chars), numChars })).leakRef();
}

JSStringRef JSStringRetain(JSStringRef string)
{
    string->ref();
    return string;
}

void JSStringRelease(JSStringRef string)
{
    string->deref();
}

size_t JSStringGetLength(JSStringRef string)
{
    if (!string)
        return 0;
    return string->length();
}

const JSChar* JSStringGetCharactersPtr(JSStringRef string)
{
    if (!string)
        return nullptr;
    return reinterpret_cast<const JSChar*>(string->characters());
}

size_t JSStringGetMaximumUTF8CStringSize(JSStringRef string)
{
    // Any UTF8 character > 3 bytes encodes as a UTF16 surrogate pair.
    return string->length() * 3 + 1; // + 1 for terminating '\0'
}

size_t JSStringGetUTF8CString(JSStringRef string, char* buffer, size_t bufferSize)
{
    if (!string || !buffer || !bufferSize)
        return 0;

    char* destination = buffer;
    bool failed = false;
    if (string->is8Bit())
        convertLatin1ToUTF8(string->span8(), &destination, destination + bufferSize - 1);
    else {
        auto characters = string->span16();
        auto result = convertUTF16ToUTF8(characters, &destination, destination + bufferSize - 1);
        failed = result != ConversionResult::Success && result != ConversionResult::TargetExhausted;
    }

    *destination++ = '\0';
    return failed ? 0 : destination - buffer;
}

bool JSStringIsEqual(JSStringRef a, JSStringRef b)
{
    return OpaqueJSString::equal(a, b);
}

bool JSStringIsEqualToUTF8CString(JSStringRef a, const char* b)
{
    return JSStringIsEqual(a, adoptRef(JSStringCreateWithUTF8CString(b)).get());
}
