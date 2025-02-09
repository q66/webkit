/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)

#include "config.h"
#include "UnifiedTextReplacementController.h"

#include "WebPage.h"
#include "WebUnifiedTextReplacementContextData.h"
#include <WebCore/BoundaryPoint.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/HTMLConverter.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextIterator.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/WebContentReader.h>

namespace WebKit {

static std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerByUUID(WebCore::Document& document, const WTF::UUID& replacementUUID)
{
    RefPtr<WebCore::Node> targetNode;
    WeakPtr<WebCore::DocumentMarker> targetMarker;

    document.markers().forEachOfTypes({ WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&replacementUUID, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());
        if (data.uuid != replacementUUID)
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

static std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerContainingRange(WebCore::Document& document, const WebCore::SimpleRange& range)
{
    RefPtr<WebCore::Node> targetNode;
    WeakPtr<WebCore::DocumentMarker> targetMarker;

    document.markers().forEachOfTypes({ WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&range, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto markerRange = WebCore::makeSimpleRange(node, marker);
        if (!WebCore::contains(WebCore::TreeType::ComposedTree, markerRange, range))
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

UnifiedTextReplacementController::UnifiedTextReplacementController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void UnifiedTextReplacementController::willBeginTextReplacementSession(const WTF::UUID& uuid, WebUnifiedTextReplacementType type, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&& completionHandler)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s)", uuid.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = m_webPage->autocorrectionContextRange();
    if (!contextRange) {
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => no context range", uuid.toString().utf8().data());
        completionHandler({ });
        return;
    }

    // If the UUID is invalid, the session is ephemeral.
    if (uuid.isValid()) {
        auto liveRange = createLiveRange(*contextRange);

        ASSERT(!m_contextRanges.contains(uuid));
        m_contextRanges.set(uuid, liveRange);

        m_replacementTypes.set(uuid, type);
    }

    auto selectedTextRange = document->selection().selection().firstRange();

    auto attributedStringFromRange = WebCore::editingAttributedString(*contextRange);
    auto selectedTextCharacterRange = WebCore::characterRange(*contextRange, *selectedTextRange);

    auto attributedStringCharacterCount = attributedStringFromRange.string.length();
    auto contextRangeCharacterCount = WebCore::characterCount(*contextRange);

    // Postcondition: the selected text character range must be a valid range within the
    // attributed string formed by the context range; the length of the entire context range
    // being equal to the length of the attributed string implies the range is valid.
    if (UNLIKELY(attributedStringCharacterCount != contextRangeCharacterCount)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => attributed string length (%u) != context range length (%llu)", uuid.toString().utf8().data(), attributedStringCharacterCount, contextRangeCharacterCount);
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void UnifiedTextReplacementController::didBeginTextReplacementSession(const WTF::UUID& uuid, const Vector<WebUnifiedTextReplacementContextData>& contexts)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didBeginTextReplacementSession (%s) [received contexts: %zu]", uuid.toString().utf8().data(), contexts.size());
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements(const WTF::UUID& uuid, const Vector<WebTextReplacementData>& replacements, const WebUnifiedTextReplacementContextData& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements (%s) [received replacements: %zu, finished: %d]", uuid.toString().utf8().data(), replacements.size(), finished);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(m_contextRanges.contains(uuid));

    m_webPage->removeTextIndicatorStyleForID(uuid);

    document->selection().clear();

    int additionalOffset = 0;

    for (const auto& replacementData : replacements) {
        auto sessionRange = contextRangeForSessionWithUUID(uuid);
        if (!sessionRange) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto locationWithOffset = replacementData.originalRange.location + additionalOffset;

        auto resolvedRange = resolveCharacterRange(*sessionRange, { locationWithOffset, replacementData.originalRange.length });

        replaceContentsOfRangeInSession(uuid, resolvedRange, replacementData.replacement);

        auto newRangeWithOffset = WebCore::CharacterRange { locationWithOffset, replacementData.replacement.length() };
        auto newResolvedRange = resolveCharacterRange(*sessionRange, newRangeWithOffset);

        auto originalString = [context.attributedText.nsAttributedString() attributedSubstringFromRange:replacementData.originalRange];

        auto markerData = WebCore::DocumentMarker::UnifiedTextReplacementData { originalString.string, replacementData.uuid, uuid, WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending };
        addMarker(newResolvedRange, WebCore::DocumentMarker::Type::UnifiedTextReplacement, markerData);

        additionalOffset += static_cast<int>(replacementData.replacement.length()) - static_cast<int>(replacementData.originalRange.length);
    }

    if (finished) {
        auto sessionRange = contextRangeForSessionWithUUID(uuid);
        if (!sessionRange) {
            ASSERT_NOT_REACHED();
            return;
        }

        document->selection().setSelection({ *sessionRange });
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement(const WTF::UUID& uuid, WebTextReplacementData::State state, const WebTextReplacementData& replacement, const WebUnifiedTextReplacementContextData& context)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement (%s) [new state: %hhu, replacement: %s]", uuid.toString().utf8().data(), enumToUnderlyingType(state), replacement.uuid.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (state != WebTextReplacementData::State::Committed && state != WebTextReplacementData::State::Reverted && state != WebTextReplacementData::State::Active)
        return;

    auto nodeAndMarker = findReplacementMarkerByUUID(*document, replacement.uuid);
    if (!nodeAndMarker) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& [node, marker] = *nodeAndMarker;

    auto rangeToReplace = WebCore::makeSimpleRange(node, marker);

    if (state == WebTextReplacementData::State::Active) {
        document->selection().setSelection({ rangeToReplace });

        auto rect = document->view()->contentsToRootView(WebCore::unionRect(WebCore::RenderObject::absoluteTextRects(rangeToReplace)));
        m_webPage->textReplacementSessionShowInformationForReplacementWithUUIDRelativeToRect(uuid, replacement.uuid, rect);

        return;
    }

    auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

    auto offsetRange = WebCore::OffsetRange { marker.startOffset(), marker.endOffset() };
    document->markers().removeMarkers(node, offsetRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

    auto newText = [&] {
        switch (state) {
        case WebTextReplacementData::State::Committed:
            return replacement.replacement;

        case WebTextReplacementData::State::Reverted:
            return data.originalText;

        default:
            ASSERT_NOT_REACHED();
            return nullString();
        }
    }();

    replaceContentsOfRangeInSession(uuid, rangeToReplace, newText);
}

void UnifiedTextReplacementController::didEndTextReplacementSession(const WTF::UUID& uuid, bool accepted)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didEndTextReplacementSession (%s) [accepted: %d]", uuid.toString().utf8().data(), accepted);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME: Associate the markers with a specific session, and only modify the markers which belong
    // to this session.

    if (!accepted) {
        Vector<std::pair<WebCore::SimpleRange, String>> replacements;

        document->markers().forEachOfTypes({ WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&replacements] (auto& node, auto& marker) {
            auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());
            if (data.state == WebCore::DocumentMarker::UnifiedTextReplacementData::State::Reverted)
                return false;

            auto rangeToReplace = makeSimpleRange(node, marker);
            replacements.append({ rangeToReplace, data.originalText });

            return false;
        });

        for (const auto& [range, text] : replacements)
            replaceContentsOfRangeInSession(uuid, range, text);
    }

    document->selection().setSelection({ *sessionRange });

    document->markers().removeMarkers({ WebCore::DocumentMarker::Type::UnifiedTextReplacement });

    m_replacementTypes.remove(uuid);
    m_contextRanges.remove(uuid);
    m_originalDocumentNodes.remove(uuid);
    m_replacedDocumentNodes.remove(uuid);
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange(const WTF::UUID& uuid, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebUnifiedTextReplacementContextData& context)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) [range: %llu, %llu]", uuid.toString().utf8().data(), range.location, range.length);

    auto contextTextCharacterCount = context.attributedText.string.length();

    // Precondition: the range is always relative to the context's attributed text, so by definition it must
    // be strictly less than the length of the attributed string.
    if (UNLIKELY(contextTextCharacterCount < range.location + range.length)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => trying to replace a range larger than the context range (context range length: %u, range.location %llu, range.length %llu)", uuid.toString().utf8().data(), contextTextCharacterCount, range.location, range.length);
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_webPage->removeTextIndicatorStyleForID(uuid);

    document->selection().clear();

    auto sessionRangeCharacterCount = WebCore::characterCount(*sessionRange);

    if (UNLIKELY(range.length + sessionRangeCharacterCount < contextTextCharacterCount)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => the range offset by the character count delta must have a non-negative size (context range length: %u, range.length %llu, session length: %llu)", uuid.toString().utf8().data(), contextTextCharacterCount, range.length, sessionRangeCharacterCount);
        ASSERT_NOT_REACHED();
        return;
    }

    // The character count delta is `sessionRangeCharacterCount - contextTextCharacterCount`;
    // the above check ensures that the full range length expression will never underflow.

    auto resolvedRange = resolveCharacterRange(*sessionRange, { range.location, range.length + sessionRangeCharacterCount - contextTextCharacterCount });

    if (!m_originalDocumentNodes.contains(uuid)) {
        auto contents = m_contextRanges.get(uuid)->cloneContents();
        if (contents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => exception when cloning contents", uuid.toString().utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }

        m_originalDocumentNodes.set(uuid, contents.returnValue()); // Deep clone.
    }

    RefPtr fragment = WebCore::createFragment(*document->frame(), attributedText.nsAttributedString().get(), WebCore::AddResources::No);
    if (!fragment) {
        ASSERT_NOT_REACHED();
        return;
    }

    replaceContentsOfRangeInSession(uuid, resolvedRange, *fragment);
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction(const WTF::UUID& uuid, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) [action: %hhu]", uuid.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacementType = m_replacementTypes.get(uuid);
    switch (replacementType) {
    case WebUnifiedTextReplacementType::PlainText:
        return textReplacementSessionPerformEditActionForPlainText(*document, uuid, action);
    case WebUnifiedTextReplacementType::RichText:
        return textReplacementSessionPerformEditActionForRichText(*document, uuid, action);
    }
}

void UnifiedTextReplacementController::textReplacementSessionPerformEditActionForPlainText(WebCore::Document& document, const WTF::UUID& uuid, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionPerformEditActionForPlainText (%s) [action: %hhu]", uuid.toString().utf8().data(), enumToUnderlyingType(action));

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<Ref<WebCore::Node>> sessionNodes;
    for (Ref node : intersectingNodes(*sessionRange))
        sessionNodes.append(node);

    for (auto nodeIterator = sessionNodes.rbegin(); nodeIterator != sessionNodes.rend(); ++nodeIterator) {
        auto node = *nodeIterator;
        auto markers = document.markers().markersFor(node, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

        for (auto markerIterator = markers.rbegin(); markerIterator != markers.rend(); ++markerIterator) {
            auto marker = *markerIterator;
            if (!marker)
                continue;

            auto rangeToReplace = makeSimpleRange(node, *marker);

            auto currentText = WebCore::plainText(rangeToReplace);

            auto oldData = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker->data());
            auto previousText = oldData.originalText;
            auto offsetRange = WebCore::OffsetRange { marker->startOffset(), marker->endOffset() };

            document.markers().removeMarkers(node, offsetRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

            auto newState = [&] {
                switch (action) {
                case WebTextReplacementData::EditAction::Undo:
                    return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Reverted;

                case WebTextReplacementData::EditAction::Redo:
                    return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending;

                default:
                    ASSERT_NOT_REACHED();
                    return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending;
                }
            }();

            replaceContentsOfRangeInSession(uuid, rangeToReplace, previousText);

            auto newData = WebCore::DocumentMarker::UnifiedTextReplacementData { currentText, oldData.uuid, uuid, newState };
            auto newOffsetRange = WebCore::OffsetRange { offsetRange.start, offsetRange.end + previousText.length() - currentText.length() };

            document.markers().addMarker(node, WebCore::DocumentMarker { WebCore::DocumentMarker::Type::UnifiedTextReplacement, newOffsetRange, WTFMove(newData) });
        }
    }
}

void UnifiedTextReplacementController::textReplacementSessionPerformEditActionForRichText(WebCore::Document& document, const WTF::UUID& uuid, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionPerformEditActionForRichText (%s) [action: %hhu]", uuid.toString().utf8().data(), enumToUnderlyingType(action));

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_originalDocumentNodes.isEmpty())
        return;

    auto contents = m_contextRanges.get(uuid)->cloneContents();
    if (contents.hasException()) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents", uuid.toString().utf8().data());
        return;
    }

    switch (action) {
    case WebTextReplacementData::EditAction::Undo: {
        RefPtr originalFragment = m_originalDocumentNodes.take(uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(uuid, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(uuid, *sessionRange, *originalFragment);

        break;
    }

    case WebTextReplacementData::EditAction::Redo: {
        RefPtr originalFragment = m_replacedDocumentNodes.take(uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(uuid, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(uuid, *sessionRange, *originalFragment);

        break;
    }

    case WebTextReplacementData::EditAction::UndoAll: {
        RefPtr originalFragment = m_originalDocumentNodes.take(uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        replaceContentsOfRangeInSession(uuid, *sessionRange, *originalFragment);
        m_replacedDocumentNodes.remove(uuid);

        break;
    }
    }

    RefPtr updatedLiveRange = m_contextRanges.get(uuid);
    if (!updatedLiveRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (action) {
    case WebTextReplacementData::EditAction::Undo:
    case WebTextReplacementData::EditAction::UndoAll: {
        auto updatedContents = updatedLiveRange->cloneContents();
        if (updatedContents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents after action", uuid.toString().utf8().data());
            return;
        }

        m_originalDocumentNodes.set(uuid, updatedContents.returnValue()); // Deep clone.

        break;
    }

    case WebTextReplacementData::EditAction::Redo:
        break;
    }
}

void UnifiedTextReplacementController::updateStateForSelectedReplacementIfNeeded()
{
    // Optimization: If there are no ongoing sessions, there is no need for any of this logic to
    // be executed, since there will be no relevant document markers anyways.
    if (m_contextRanges.isEmpty())
        return;

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto selectionRange = document->selection().selection().firstRange();
    if (!selectionRange)
        return;

    if (!document->selection().isCaret())
        return;

    auto nodeAndMarker = findReplacementMarkerContainingRange(*document, *selectionRange);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;
    auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

    m_webPage->textReplacementSessionUpdateStateForReplacementWithUUID(data.sessionUUID, WebTextReplacementData::State::Active, data.uuid);
}

// MARK: Private instance helper methods.

std::optional<WebCore::SimpleRange> UnifiedTextReplacementController::contextRangeForSessionWithUUID(const WTF::UUID& uuid) const
{
    return WebCore::makeSimpleRange(m_contextRanges.get(uuid));
}

RefPtr<WebCore::Document> UnifiedTextReplacementController::document() const
{
    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr frame = corePage->checkedFocusController()->focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return frame->document();
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSessionInternal(const WTF::UUID& uuid, const WebCore::SimpleRange& range, WTF::Function<void(WebCore::Editor&)>&& replacementOperation)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRangeCount = WebCore::characterCount(*sessionRange);

    auto resolvedCharacterRange = WebCore::characterRange(*sessionRange, range);
    document->selection().setSelection({ range });

    replacementOperation(document->editor());

    auto selectedTextRange = document->selection().selection().firstRange();
    if (!selectedTextRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto extendedPosition = [](const WebCore::BoundaryPoint& point, uint64_t characterCount, WebCore::SelectionDirection direction) {
        auto visiblePosition = WebCore::VisiblePosition { WebCore::makeContainerOffsetPosition(point) };

        for (uint64_t i = 0; i < characterCount; ++i) {
            auto nextVisiblePosition = WebCore::positionOfNextBoundaryOfGranularity(visiblePosition, WebCore::TextGranularity::CharacterGranularity, direction);
            if (nextVisiblePosition.isNull())
                break;

            visiblePosition = nextVisiblePosition;
        }

        return visiblePosition;
    };

    auto extendedSelection = [extendedPosition](const WebCore::SimpleRange& range, uint64_t charactersToExtendBackwards, uint64_t charactersToExtendForwards) {
        auto start = extendedPosition(range.start, charactersToExtendBackwards, WebCore::SelectionDirection::Backward);
        auto end = extendedPosition(range.end, charactersToExtendForwards, WebCore::SelectionDirection::Forward);

        return WebCore::makeSimpleRange(start, end);
    };

    auto newSessionRange = extendedSelection(*selectedTextRange, resolvedCharacterRange.location, sessionRangeCount - (resolvedCharacterRange.location + resolvedCharacterRange.length));
    if (!newSessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto updatedLiveRange = WebCore::createLiveRange(*newSessionRange);
    m_contextRanges.set(uuid, updatedLiveRange);
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const WTF::UUID& uuid, const WebCore::SimpleRange& range, const String& replacementText)
{
    replaceContentsOfRangeInSessionInternal(uuid, range, [&replacementText](WebCore::Editor& editor) {
        editor.replaceSelectionWithText(replacementText, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No, WebCore::EditAction::InsertReplacement);
    });
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const WTF::UUID& uuid, const WebCore::SimpleRange& range, WebCore::DocumentFragment& fragment)
{
    replaceContentsOfRangeInSessionInternal(uuid, range, [&fragment](WebCore::Editor& editor) {
        editor.replaceSelectionWithFragment(fragment, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No, WebCore::Editor::MatchStyle::No, WebCore::EditAction::InsertReplacement);
    });
}

} // namespace WebKit

#endif
