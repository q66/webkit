/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IDLTypes.h"
#include "JSDOMConvertBase.h"
#include "StringAdaptors.h"
#include "TrustedType.h"

namespace WebCore {

class ScriptExecutionContext;

enum class ShouldConvertNullToEmptyString : bool { No, Yes };

WEBCORE_EXPORT String identifierToString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT String identifierToByteString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToByteString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT AtomString valueToByteAtomString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT String identifierToUSVString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToUSVString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT AtomString valueToUSVAtomString(JSC::JSGlobalObject&, JSC::JSValue);
String trustedTypeCompliantString(TrustedType, JSC::JSGlobalObject&, JSC::JSValue, const String& sink, ShouldConvertNullToEmptyString);

inline AtomString propertyNameToString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.uid() ? propertyName.uid() : propertyName.publicName();
}

inline AtomString propertyNameToAtomString(JSC::PropertyName propertyName)
{
    return AtomString(propertyName.uid() ? propertyName.uid() : propertyName.publicName());
}

// MARK: -
// MARK: String types

template<> struct Converter<IDLDOMString> : DefaultConverter<IDLDOMString> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return value.toWTFString(&lexicalGlobalObject);
    }
};

template<> struct JSConverter<IDLDOMString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

template<> struct Converter<IDLByteString> : DefaultConverter<IDLByteString> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToByteString(lexicalGlobalObject, value);
    }
};

template<> struct JSConverter<IDLByteString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }
};

template<> struct Converter<IDLUSVString> : DefaultConverter<IDLUSVString> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToUSVString(lexicalGlobalObject, value);
    }
    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

template<> struct JSConverter<IDLUSVString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

// MARK: -
// MARK: String type adaptors

template<typename T> struct Converter<IDLLegacyNullToEmptyStringAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyStringAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyString();
        return Converter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLStringContextTrustedHTMLAdaptor<T>> : DefaultConverter<IDLStringContextTrustedHTMLAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLStringContextTrustedHTMLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedHTML, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);
    }
};

template<typename T> struct JSConverter<IDLStringContextTrustedHTMLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedHTML, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::Yes);
    }
};

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLStringContextTrustedScriptAdaptor<T>> : DefaultConverter<IDLStringContextTrustedScriptAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLStringContextTrustedScriptAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedScript, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);
    }
};

template<typename T> struct JSConverter<IDLStringContextTrustedScriptAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedScript, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::Yes);
    }
};

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLStringContextTrustedScriptURLAdaptor<T>> : DefaultConverter<IDLStringContextTrustedScriptURLAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLStringContextTrustedScriptURLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedScriptURL, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);
    }
};

template<typename T> struct JSConverter<IDLStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> {
    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static String convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(TrustedType::TrustedScriptURL, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::Yes);
    }
};

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> : DefaultConverter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        auto result = trustedTypeCompliantString(TrustedType::TrustedHTML, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);

        return AtomString { result };
    }
};

template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLAtomStringStringContextTrustedScriptAdaptor<T>> : DefaultConverter<IDLAtomStringStringContextTrustedScriptAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLAtomStringStringContextTrustedScriptAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        auto result = trustedTypeCompliantString(TrustedType::TrustedScript, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);

        return AtomString { result };
    }
};

template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedScriptAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> : DefaultConverter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>>::convert(lexicalGlobalObject, value, emptyString());
    }

    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        auto result = trustedTypeCompliantString(TrustedType::TrustedScriptURL, lexicalGlobalObject, value, sink, ShouldConvertNullToEmptyString::No);

        return AtomString { result };
    }
};

template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyAtom();
        return Converter<IDLAtomStringAdaptor<T>>::convert(lexicalGlobalObject, value);
    }
};

template<typename T>  struct JSConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLAtomStringAdaptor<T>> : DefaultConverter<IDLAtomStringAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);
    }
};

template<> struct Converter<IDLAtomStringAdaptor<IDLUSVString>> : DefaultConverter<IDLAtomStringAdaptor<IDLUSVString>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToUSVAtomString(lexicalGlobalObject, value);
    }
};

template<> struct Converter<IDLAtomStringAdaptor<IDLByteString>> : DefaultConverter<IDLAtomStringAdaptor<IDLByteString>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToByteAtomString(lexicalGlobalObject, value);
    }
};

template<typename T>  struct JSConverter<IDLAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value.string());
    }
};

template<>  struct JSConverter<IDLAtomStringAdaptor<IDLUSVString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value.string());
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value.string());
    }
};

template<>  struct JSConverter<IDLAtomStringAdaptor<IDLByteString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLRequiresExistingAtomStringAdaptor<T>> : DefaultConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");
    
        return value.toString(&lexicalGlobalObject)->toExistingAtomString(&lexicalGlobalObject);
    }
};

template<typename T>  struct JSConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};


} // namespace WebCore
