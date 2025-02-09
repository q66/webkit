/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "OffscreenCanvas.h"

#if ENABLE(OFFSCREEN_CANVAS)

#include "BitmapImage.h"
#include "CSSParserContext.h"
#include "CSSValuePool.h"
#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "Document.h"
#include "EventDispatcher.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "HTMLCanvasElement.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageData.h"
#include "JSBlob.h"
#include "JSDOMPromiseDeferred.h"
#include "MIMETypeRegistry.h"
#include "OffscreenCanvasRenderingContext2D.h"
#include "Page.h"
#include "PlaceholderRenderingContext.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include "WorkerNavigator.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(WEBGL)
#include "Settings.h"
#include "WebGLRenderingContext.h"
#include "WebGL2RenderingContext.h"
#endif // ENABLE(WEBGL)

#if HAVE(WEBGPU_IMPLEMENTATION)
#include "LocalDomWindow.h"
#include "Navigator.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OffscreenCanvas);

class OffscreenCanvasPlaceholderData : public ThreadSafeRefCounted<OffscreenCanvasPlaceholderData, WTF::DestructionThread::Main> {
    WTF_MAKE_NONCOPYABLE(OffscreenCanvasPlaceholderData);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<OffscreenCanvasPlaceholderData> create(HTMLCanvasElement& placeholder)
    {
        RefPtr<ImageBufferPipe::Source> pipeSource;
        RefPtr placeholderContext = downcast<PlaceholderRenderingContext>(placeholder.renderingContext());
        if (auto& pipe = placeholderContext->imageBufferPipe())
            pipeSource = pipe->source();
        return adoptRef(*new OffscreenCanvasPlaceholderData { placeholder, WTFMove(pipeSource) });
    }
    RefPtr<HTMLCanvasElement> placeholder() const { return m_placeholder.get(); }
    RefPtr<ImageBufferPipe::Source> pipeSource() const { return m_pipeSource; }

private:
    OffscreenCanvasPlaceholderData(HTMLCanvasElement& placeholder, RefPtr<ImageBufferPipe::Source> pipeSource)
        : m_placeholder(placeholder)
        , m_pipeSource(WTFMove(pipeSource))
    {
    }

    WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> m_placeholder;
    RefPtr<ImageBufferPipe::Source> m_pipeSource;
};

DetachedOffscreenCanvas::DetachedOffscreenCanvas(std::unique_ptr<SerializedImageBuffer> buffer, const IntSize& size, bool originClean, RefPtr<OffscreenCanvasPlaceholderData> placeholderData)
    : m_buffer(WTFMove(buffer))
    , m_placeholderData(WTFMove(placeholderData))
    , m_size(size)
    , m_originClean(originClean)
{
}

DetachedOffscreenCanvas::~DetachedOffscreenCanvas() = default;

RefPtr<ImageBuffer> DetachedOffscreenCanvas::takeImageBuffer(ScriptExecutionContext& context)
{
    if (!m_buffer)
        return nullptr;
    return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(m_buffer), context.graphicsClient());
}

RefPtr<OffscreenCanvasPlaceholderData> DetachedOffscreenCanvas::takePlaceholderData()
{
    return WTFMove(m_placeholderData);
}

bool OffscreenCanvas::enabledForContext(ScriptExecutionContext& context)
{
    UNUSED_PARAM(context);

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    if (context.isWorkerGlobalScope())
        return context.settingsValues().offscreenCanvasInWorkersEnabled;
#endif

    ASSERT(context.isDocument());
    return true;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, unsigned width, unsigned height)
{
    auto canvas = adoptRef(*new OffscreenCanvas(scriptExecutionContext, { static_cast<int>(width), static_cast<int>(height) }, nullptr));
    canvas->suspendIfNeeded();
    return canvas;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, std::unique_ptr<DetachedOffscreenCanvas>&& detachedCanvas)
{
    Ref<OffscreenCanvas> clone = adoptRef(*new OffscreenCanvas(scriptExecutionContext, detachedCanvas->size(), detachedCanvas->takePlaceholderData()));
    clone->setImageBuffer(detachedCanvas->takeImageBuffer(scriptExecutionContext));
    if (!detachedCanvas->originClean())
        clone->setOriginTainted();
    clone->suspendIfNeeded();
    return clone;
}

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& scriptExecutionContext, HTMLCanvasElement& placeholder)
{
    auto offscreen = adoptRef(*new OffscreenCanvas(scriptExecutionContext, placeholder.size(), OffscreenCanvasPlaceholderData::create(placeholder)));
    offscreen->suspendIfNeeded();
    return offscreen;
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& scriptExecutionContext, IntSize size, RefPtr<OffscreenCanvasPlaceholderData> placeholderData)
    : ActiveDOMObject(&scriptExecutionContext)
    , CanvasBase(WTFMove(size), scriptExecutionContext.noiseInjectionHashSalt())
    , m_placeholderData(WTFMove(placeholderData))
{
}

OffscreenCanvas::~OffscreenCanvas()
{
    notifyObserversCanvasDestroyed();
    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_context = nullptr; // Ensure this goes away before the ImageBuffer.
    setImageBuffer(nullptr);
}

void OffscreenCanvas::setWidth(unsigned newWidth)
{
    if (m_detached)
        return;
    setSize(IntSize(newWidth, height()));
}

void OffscreenCanvas::setHeight(unsigned newHeight)
{
    if (m_detached)
        return;
    setSize(IntSize(width(), newHeight));
}

void OffscreenCanvas::setSize(const IntSize& newSize)
{
    auto oldWidth = width();
    auto oldHeight = height();
    CanvasBase::setSize(newSize);
    reset();

    if (RefPtr context = dynamicDowncast<GPUBasedCanvasRenderingContext>(m_context.get()))
        context->reshape(width(), height(), oldWidth, oldHeight);
}

#if ENABLE(WEBGL)
static bool requiresAcceleratedCompositingForWebGL()
{
#if PLATFORM(GTK) || PLATFORM(WIN)
    return false;
#else
    return true;
#endif
}

static bool shouldEnableWebGL(const Settings::Values& settings, bool isWorker)
{
    if (!settings.webGLEnabled)
        return false;

    if (!settings.allowWebGLInWorkers)
        return false;

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    if (isWorker && !settings.useGPUProcessForWebGLEnabled)
        return false;
#else
    UNUSED_PARAM(isWorker);
#endif

    if (!requiresAcceleratedCompositingForWebGL())
        return true;

    return settings.acceleratedCompositingEnabled;
}

#endif // ENABLE(WEBGL)

ExceptionOr<std::optional<OffscreenRenderingContext>> OffscreenCanvas::getContext(JSC::JSGlobalObject& state, RenderingContextType contextType, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_detached)
        return Exception { ExceptionCode::InvalidStateError };

    if (contextType == RenderingContextType::_2d) {
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            auto settings = convert<IDLDictionary<CanvasRenderingContext2DSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            m_context = OffscreenCanvasRenderingContext2D::create(*this, WTFMove(settings));
        }
        if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
            return { { WTFMove(context) } };
        return { { std::nullopt } };
    }
    if (contextType == RenderingContextType::Bitmaprenderer) {
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            auto settings = convert<IDLDictionary<ImageBitmapRenderingContextSettings>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            m_context = ImageBitmapRenderingContext::create(*this, WTFMove(settings));
            downcast<ImageBitmapRenderingContext>(m_context.get())->transferFromImageBitmap(nullptr);
        }
        if (RefPtr context = dynamicDowncast<ImageBitmapRenderingContext>(m_context.get()))
            return { { WTFMove(context) } };
        return { { std::nullopt } };
    }
    if (contextType == RenderingContextType::Webgpu) {
#if HAVE(WEBGPU_IMPLEMENTATION)
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            Ref scriptExecutionContext = *this->scriptExecutionContext();
            if (RefPtr globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext)) {
                if (auto* gpu = globalScope->navigator().gpu())
                    m_context = GPUCanvasContext::create(*this, *gpu);
            } else if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
                if (RefPtr domWindow = document->domWindow()) {
                    if (auto* gpu = domWindow->navigator().gpu())
                        m_context = GPUCanvasContext::create(*this, *gpu);
                }
            }
        }
        if (RefPtr context = dynamicDowncast<GPUCanvasContext>(m_context.get()))
            return { { WTFMove(context) } };
#endif
        return { { std::nullopt } };
    }
#if ENABLE(WEBGL)
    if (contextType == RenderingContextType::Webgl || contextType == RenderingContextType::Webgl2) {
        auto webGLVersion = contextType == RenderingContextType::Webgl ? WebGLVersion::WebGL1 : WebGLVersion::WebGL2;
        if (!m_context) {
            auto scope = DECLARE_THROW_SCOPE(state.vm());
            auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, arguments.isEmpty() ? JSC::jsUndefined() : (arguments[0].isObject() ? arguments[0].get() : JSC::jsNull()));
            RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
            auto* scriptExecutionContext = this->scriptExecutionContext();
            if (shouldEnableWebGL(scriptExecutionContext->settingsValues(), is<WorkerGlobalScope>(scriptExecutionContext)))
                m_context = WebGLRenderingContextBase::create(*this, attributes, webGLVersion);
        }
        if (webGLVersion == WebGLVersion::WebGL1) {
            if (RefPtr context = dynamicDowncast<WebGLRenderingContext>(m_context.get()))
                return { { WTFMove(context) } };
        } else {
            if (RefPtr context = dynamicDowncast<WebGL2RenderingContext>(m_context.get()))
                return { { WTFMove(context) } };
        }
        return { { std::nullopt } };
    }
#endif

    return Exception { ExceptionCode::TypeError };
}

ExceptionOr<RefPtr<ImageBitmap>> OffscreenCanvas::transferToImageBitmap()
{
    if (m_detached || !m_context)
        return Exception { ExceptionCode::InvalidStateError };

    if (is<OffscreenCanvasRenderingContext2D>(*m_context) || is<ImageBitmapRenderingContext>(*m_context)) {
        if (!width() || !height())
            return { RefPtr<ImageBitmap> { nullptr } };

        if (!m_hasCreatedImageBuffer) {
            auto buffer = allocateImageBuffer();
            if (!buffer)
                return { RefPtr<ImageBitmap> { nullptr } };
            return { ImageBitmap::create(buffer.releaseNonNull(), originClean()) };
        }

        if (!buffer())
            return { RefPtr<ImageBitmap> { nullptr } };

        RefPtr<ImageBuffer> bitmap;
        if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(*m_context)) {
            // As the canvas context state is stored in GraphicsContext, which is owned
            // by buffer(), to avoid resetting the context state, we have to make a copy and
            // clear the original buffer rather than returning the original buffer.
            bitmap = buffer()->clone();
            context->clearCanvas();
        } else {
            // ImageBitmapRenderingContext doesn't use the context state, so we can just take its
            // buffer, and then call transferFromImageBitmap(nullptr) which will trigger it to allocate
            // a new blank bitmap.
            bitmap = buffer();
            downcast<ImageBitmapRenderingContext>(*m_context).transferFromImageBitmap(nullptr);
        }
        clearCopiedImage();
        if (!bitmap)
            return { RefPtr<ImageBitmap> { nullptr } };
        return { ImageBitmap::create(bitmap.releaseNonNull(), originClean(), false, false) };
    }

#if ENABLE(WEBGL)
    if (auto* webGLContext = dynamicDowncast<WebGLRenderingContextBase>(*m_context)) {
        // FIXME: We're supposed to create an ImageBitmap using the backing
        // store from this canvas (or its context), but for now we'll just
        // create a new bitmap and paint into it.
        auto buffer = allocateImageBuffer();
        if (!buffer)
            return { RefPtr<ImageBitmap> { nullptr } };

        RefPtr gc3d = webGLContext->graphicsContextGL();
        gc3d->drawSurfaceBufferToImageBuffer(GraphicsContextGL::SurfaceBuffer::DrawingBuffer, *buffer);

        // FIXME: The transfer algorithm requires that the canvas effectively
        // creates a new backing store. Since we're not doing that yet, we
        // need to erase what's there.

        GCGLfloat clearColor[4] { };
        gc3d->getFloatv(GraphicsContextGL::COLOR_CLEAR_VALUE, clearColor);
        gc3d->clearColor(0, 0, 0, 0);
        gc3d->clear(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT);
        gc3d->clearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        return { ImageBitmap::create(buffer.releaseNonNull(), originClean()) };
    }
#endif

    if (auto* context = dynamicDowncast<GPUCanvasContext>(*m_context)) {
        auto buffer = allocateImageBuffer();
        if (!buffer)
            return Exception { ExceptionCode::OutOfMemoryError };

        Ref<ImageBuffer> bufferRef = buffer.releaseNonNull();
        return context->getCurrentTextureAsImageBitmap(bufferRef, originClean());
    }

    return Exception { ExceptionCode::NotSupportedError };
}

static String toEncodingMimeType(const String& mimeType)
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "image/png"_s;
    return mimeType.convertToASCIILowercase();
}

static std::optional<double> qualityFromDouble(double qualityNumber)
{
    if (!(qualityNumber >= 0 && qualityNumber <= 1))
        return std::nullopt;

    return qualityNumber;
}

void OffscreenCanvas::convertToBlob(ImageEncodeOptions&& options, Ref<DeferredPromise>&& promise)
{
    if (!originClean()) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }
    if (m_detached) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    if (size().isEmpty()) {
        promise->reject(ExceptionCode::IndexSizeError);
        return;
    }
    if (!buffer()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    makeRenderingResultsAvailable();

    auto encodingMIMEType = toEncodingMimeType(options.type);
    auto quality = qualityFromDouble(options.quality);

    Vector<uint8_t> blobData = buffer()->toData(encodingMIMEType, quality);
    if (blobData.isEmpty()) {
        promise->reject(ExceptionCode::EncodingError);
        return;
    }

    Ref<Blob> blob = Blob::create(canvasBaseScriptExecutionContext(), WTFMove(blobData), encodingMIMEType);
    promise->resolveWithNewlyCreated<IDLInterface<Blob>>(WTFMove(blob));
}

void OffscreenCanvas::didDraw(const std::optional<FloatRect>& rect, ShouldApplyPostProcessingToDirtyRect shouldApplyPostProcessingToDirtyRect)
{
    clearCopiedImage();
    scheduleCommitToPlaceholderCanvas();
    CanvasBase::didDraw(rect, shouldApplyPostProcessingToDirtyRect);
}

Image* OffscreenCanvas::copiedImage() const
{
    if (m_detached)
        return nullptr;

    if (!m_copiedImage && buffer()) {
        if (m_context)
            m_context->drawBufferToCanvas(CanvasRenderingContext::SurfaceBuffer::DrawingBuffer);
        m_copiedImage = BitmapImage::create(buffer()->copyNativeImage());
    }
    return m_copiedImage.get();
}

void OffscreenCanvas::clearCopiedImage() const
{
    m_copiedImage = nullptr;
}

SecurityOrigin* OffscreenCanvas::securityOrigin() const
{
    auto& scriptExecutionContext = *canvasBaseScriptExecutionContext();
    if (auto* globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext))
        return &globalScope->topOrigin();

    return &downcast<Document>(scriptExecutionContext).securityOrigin();
}

bool OffscreenCanvas::canDetach() const
{
    return !m_detached && !m_context;
}

std::unique_ptr<DetachedOffscreenCanvas> OffscreenCanvas::detach()
{
    if (!canDetach())
        return nullptr;

    removeCanvasNeedingPreparationForDisplayOrFlush();

    m_detached = true;

    auto detached = makeUnique<DetachedOffscreenCanvas>(takeImageBuffer(), size(), originClean(), WTFMove(m_placeholderData));
    setSize(IntSize(0, 0));
    return detached;
}

void OffscreenCanvas::commitToPlaceholderCanvas()
{
    RefPtr imageBuffer = buffer();
    if (!imageBuffer)
        return;
    if (!m_placeholderData)
        return;

    // FIXME: Transfer texture over if we're using accelerated compositing
    if (m_context && (m_context->isWebGL() || m_context->isAccelerated())) {
        if (m_context->compositingResultsNeedUpdating())
            m_context->prepareForDisplay();
        m_context->drawBufferToCanvas(CanvasRenderingContext::SurfaceBuffer::DisplayBuffer);
    }

    if (auto pipeSource = m_placeholderData->pipeSource())
        pipeSource->handle(*imageBuffer);

    auto clone = imageBuffer->clone();
    if (!clone)
        return;
    auto serializedClone = ImageBuffer::sinkIntoSerializedImageBuffer(WTFMove(clone));
    if (!serializedClone)
        return;
    callOnMainThread([placeholderData = Ref { *m_placeholderData }, buffer = WTFMove(serializedClone)] () mutable {
        RefPtr canvas = placeholderData->placeholder();
        if (!canvas)
            return;
        RefPtr imageBuffer = SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(buffer), canvas->document().graphicsClient());
        if (!imageBuffer)
            return;
        canvas->setImageBufferAndMarkDirty(WTFMove(imageBuffer));
    });
}

void OffscreenCanvas::scheduleCommitToPlaceholderCanvas()
{
    if (!m_hasScheduledCommit && m_placeholderData) {
        auto& scriptContext = *scriptExecutionContext();
        m_hasScheduledCommit = true;
        scriptContext.postTask([protectedThis = Ref { *this }, this] (ScriptExecutionContext&) {
            m_hasScheduledCommit = false;
            commitToPlaceholderCanvas();
        });
    }
}

void OffscreenCanvas::createImageBuffer() const
{
    m_hasCreatedImageBuffer = true;
    setImageBuffer(allocateImageBuffer());
}

void OffscreenCanvas::setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&& buffer)
{
    m_hasCreatedImageBuffer = true;
    setImageBuffer(WTFMove(buffer));

    CanvasBase::didDraw(FloatRect(FloatPoint(), size()));
}

std::unique_ptr<SerializedImageBuffer> OffscreenCanvas::takeImageBuffer() const
{
    ASSERT(m_detached);

    if (size().isEmpty())
        return nullptr;

    clearCopiedImage();
    RefPtr<ImageBuffer> buffer = setImageBuffer(nullptr);
    if (!buffer)
        return nullptr;
    return ImageBuffer::sinkIntoSerializedImageBuffer(WTFMove(buffer));
}

void OffscreenCanvas::reset()
{
    resetGraphicsContextState();
    if (RefPtr context = dynamicDowncast<OffscreenCanvasRenderingContext2D>(m_context.get()))
        context->reset();

    m_hasCreatedImageBuffer = false;
    setImageBuffer(nullptr);
    clearCopiedImage();

    notifyObserversCanvasResized();
    scheduleCommitToPlaceholderCanvas();
}

void OffscreenCanvas::queueTaskKeepingObjectAlive(TaskSource source, Function<void()>&& task)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(*this, source, WTFMove(task));
}

void OffscreenCanvas::dispatchEvent(Event& event)
{
    EventDispatcher::dispatchEvent({ this }, event);
}

const CSSParserContext& OffscreenCanvas::cssParserContext() const
{
    // FIXME: Rather than using a default CSSParserContext, there should be one exposed via ScriptExecutionContext.
    if (!m_cssParserContext)
        m_cssParserContext = WTF::makeUnique<CSSParserContext>(HTMLStandardMode);
    return *m_cssParserContext;
}

}

#endif
