/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "ColorTypes.h"
#include <functional>
#include <wtf/Forward.h>

namespace WebCore {

// Tools/lldb/lldb_webkit.py has a copy of this list, which should be kept in sync.
enum class ColorSpace : uint8_t {
    A98RGB,
    DisplayP3,
    ExtendedA98RGB,
    ExtendedDisplayP3,
    ExtendedLinearSRGB,
    ExtendedProPhotoRGB,
    ExtendedRec2020,
    ExtendedSRGB,
    HSL,
    HWB,
    LCH,
    Lab,
    LinearSRGB,
    OKLCH,
    OKLab,
    ProPhotoRGB,
    Rec2020,
    SRGB,
    XYZ_D50,
    XYZ_D65,
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, ColorSpace);


template<typename> struct ColorSpaceMapping;
template<typename T> struct ColorSpaceMapping<A98RGB<T>> { static constexpr auto colorSpace { ColorSpace::A98RGB }; };
template<typename T> struct ColorSpaceMapping<DisplayP3<T>> { static constexpr auto colorSpace { ColorSpace::DisplayP3 }; };
template<typename T> struct ColorSpaceMapping<ExtendedA98RGB<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedA98RGB }; };
template<typename T> struct ColorSpaceMapping<ExtendedDisplayP3<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedDisplayP3 }; };
template<typename T> struct ColorSpaceMapping<ExtendedLinearSRGBA<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedLinearSRGB }; };
template<typename T> struct ColorSpaceMapping<ExtendedProPhotoRGB<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedProPhotoRGB }; };
template<typename T> struct ColorSpaceMapping<ExtendedRec2020<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedRec2020 }; };
template<typename T> struct ColorSpaceMapping<ExtendedSRGBA<T>> { static constexpr auto colorSpace { ColorSpace::ExtendedSRGB }; };
template<typename T> struct ColorSpaceMapping<HSLA<T>> { static constexpr auto colorSpace { ColorSpace::HSL }; };
template<typename T> struct ColorSpaceMapping<HWBA<T>> { static constexpr auto colorSpace { ColorSpace::HWB }; };
template<typename T> struct ColorSpaceMapping<LCHA<T>> { static constexpr auto colorSpace { ColorSpace::LCH }; };
template<typename T> struct ColorSpaceMapping<Lab<T>> { static constexpr auto colorSpace { ColorSpace::Lab }; };
template<typename T> struct ColorSpaceMapping<LinearSRGBA<T>> { static constexpr auto colorSpace { ColorSpace::LinearSRGB }; };
template<typename T> struct ColorSpaceMapping<OKLab<T>> { static constexpr auto colorSpace { ColorSpace::OKLab }; };
template<typename T> struct ColorSpaceMapping<OKLCHA<T>> { static constexpr auto colorSpace { ColorSpace::OKLCH }; };
template<typename T> struct ColorSpaceMapping<ProPhotoRGB<T>> { static constexpr auto colorSpace { ColorSpace::ProPhotoRGB }; };
template<typename T> struct ColorSpaceMapping<Rec2020<T>> { static constexpr auto colorSpace { ColorSpace::Rec2020 }; };
template<typename T> struct ColorSpaceMapping<SRGBA<T>> { static constexpr auto colorSpace { ColorSpace::SRGB }; };
template<typename T> struct ColorSpaceMapping<XYZA<T, WhitePoint::D50>> { static constexpr auto colorSpace { ColorSpace::XYZ_D50 }; };
template<typename T> struct ColorSpaceMapping<XYZA<T, WhitePoint::D65>> { static constexpr auto colorSpace { ColorSpace::XYZ_D65 }; };

template<typename ColorType> constexpr ColorSpace ColorSpaceFor = ColorSpaceMapping<CanonicalColorType<ColorType>>::colorSpace;


template<typename T, typename Functor> constexpr decltype(auto) callWithColorType(const ColorComponents<T, 4>& components, ColorSpace colorSpace, Functor&& functor)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<A98RGB<T>>(components));
    case ColorSpace::DisplayP3:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<DisplayP3<T>>(components));
    case ColorSpace::ExtendedA98RGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedA98RGB<T>>(components));
    case ColorSpace::ExtendedDisplayP3:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedDisplayP3<T>>(components));
    case ColorSpace::ExtendedLinearSRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedLinearSRGBA<T>>(components));
    case ColorSpace::ExtendedProPhotoRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedProPhotoRGB<T>>(components));
    case ColorSpace::ExtendedRec2020:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedRec2020<T>>(components));
    case ColorSpace::ExtendedSRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ExtendedSRGBA<T>>(components));
    case ColorSpace::HSL:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<HSLA<T>>(components));
    case ColorSpace::HWB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<HWBA<T>>(components));
    case ColorSpace::LCH:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<LCHA<T>>(components));
    case ColorSpace::Lab:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<Lab<T>>(components));
    case ColorSpace::LinearSRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<LinearSRGBA<T>>(components));
    case ColorSpace::OKLCH:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<OKLCHA<T>>(components));
    case ColorSpace::OKLab:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<OKLab<T>>(components));
    case ColorSpace::ProPhotoRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<ProPhotoRGB<T>>(components));
    case ColorSpace::Rec2020:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<Rec2020<T>>(components));
    case ColorSpace::SRGB:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
    case ColorSpace::XYZ_D50:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<XYZA<T, WhitePoint::D50>>(components));
    case ColorSpace::XYZ_D65:
        return std::invoke(std::forward<Functor>(functor), makeFromComponents<XYZA<T, WhitePoint::D65>>(components));
    }

    ASSERT_NOT_REACHED();
    return std::invoke(std::forward<Functor>(functor), makeFromComponents<SRGBA<T>>(components));
}


} // namespace WebCore
