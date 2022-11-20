/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/ui/dialogs_message_view.h"

#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_item_preview.h"
#include "main/main_session.h"
#include "ui/text/text_options.h"
#include "ui/text/text_utilities.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "core/ui_integration.h"
#include "lang/lang_keys.h"
#include "styles/style_dialogs.h"

namespace {

template <ushort kTag>
struct TextWithTagOffset {
	TextWithTagOffset(QString text) : text(text) {
	}
	static TextWithTagOffset FromString(const QString &text) {
		return { text };
	}

	QString text;
	int offset = -1;
};

} // namespace

namespace Lang {

template <ushort kTag>
struct ReplaceTag<TextWithTagOffset<kTag>> {
	static TextWithTagOffset<kTag> Call(
		TextWithTagOffset<kTag> &&original,
		ushort tag,
		const TextWithTagOffset<kTag> &replacement);
};

template <ushort kTag>
TextWithTagOffset<kTag> ReplaceTag<TextWithTagOffset<kTag>>::Call(
		TextWithTagOffset<kTag> &&original,
		ushort tag,
		const TextWithTagOffset<kTag> &replacement) {
	const auto replacementPosition = FindTagReplacementPosition(
		original.text,
		tag);
	if (replacementPosition < 0) {
		return std::move(original);
	}
	original.text = ReplaceTag<QString>::Replace(
		std::move(original.text),
		replacement.text,
		replacementPosition);
	if (tag == kTag) {
		original.offset = replacementPosition;
	} else if (original.offset > replacementPosition) {
		constexpr auto kReplaceCommandLength = 4;
		original.offset += replacement.text.size() - kReplaceCommandLength;
	}
	return std::move(original);
}

} // namespace Lang

namespace Dialogs::Ui {

TextWithEntities DialogsPreviewText(TextWithEntities text) {
	auto result = Ui::Text::Filtered(
		std::move(text),
		{
			EntityType::Pre,
			EntityType::Code,
			EntityType::Spoiler,
			EntityType::StrikeOut,
			EntityType::Underline,
			EntityType::Italic,
			EntityType::CustomEmoji,
			EntityType::PlainLink,
		});
	for (auto &entity : result.entities) {
		if (entity.type() == EntityType::Pre) {
			entity = EntityInText(
				EntityType::Code,
				entity.offset(),
				entity.length());
		}
	}
	return result;
}

struct MessageView::LoadingContext {
	std::any context;
	rpl::lifetime lifetime;
};

MessageView::MessageView()
: _senderCache(st::dialogsTextWidthMin)
, _textCache(st::dialogsTextWidthMin) {
}

MessageView::~MessageView() = default;

void MessageView::itemInvalidated(not_null<const HistoryItem*> item) {
	if (_textCachedFor == item.get()) {
		_textCachedFor = nullptr;
	}
}

bool MessageView::dependsOn(not_null<const HistoryItem*> item) const {
	return (_textCachedFor == item.get());
}

bool MessageView::prepared(not_null<const HistoryItem*> item) const {
	return (_textCachedFor == item.get());
}

void MessageView::prepare(
		not_null<const HistoryItem*> item,
		Fn<void()> customEmojiRepaint,
		ToPreviewOptions options) {
	options.existing = &_imagesCache;
	auto preview = item->toPreview(options);
	if (!preview.images.empty() && preview.imagesInTextPosition > 0) {
		auto sender = ::Ui::Text::Mid(
			preview.text,
			0,
			preview.imagesInTextPosition);
		TextUtilities::Trim(sender);
		_senderCache.setMarkedText(
			st::dialogsTextStyle,
			std::move(sender),
			DialogTextOptions());
		preview.text = ::Ui::Text::Mid(
			preview.text,
			preview.imagesInTextPosition);
	} else {
		_senderCache = { st::dialogsTextWidthMin };
	}
	TextUtilities::Trim(preview.text);
	const auto history = item->history();
	const auto context = Core::MarkedTextContext{
		.session = &history->session(),
		.customEmojiRepaint = customEmojiRepaint,
	};
	_textCache.setMarkedText(
		st::dialogsTextStyle,
		DialogsPreviewText(std::move(preview.text)),
		DialogTextOptions(),
		context);
	_textCachedFor = item;
	_imagesCache = std::move(preview.images);
	if (preview.loadingContext.has_value()) {
		if (!_loadingContext) {
			_loadingContext = std::make_unique<LoadingContext>();
			item->history()->session().downloaderTaskFinished(
			) | rpl::start_with_next([=] {
				_textCachedFor = nullptr;
			}, _loadingContext->lifetime);
		}
		_loadingContext->context = std::move(preview.loadingContext);
	} else {
		_loadingContext = nullptr;
	}
}

void MessageView::paint(
		Painter &p,
		const QRect &geometry,
		bool active,
		bool selected,
		crl::time now,
		bool paused) const {
	if (geometry.isEmpty()) {
		return;
	}
	p.setFont(st::dialogsTextFont);
	p.setPen(active
		? st::dialogsTextFgActive
		: selected
		? st::dialogsTextFgOver
		: st::dialogsTextFg);
	const auto palette = &(active
		? st::dialogsTextPaletteActive
		: selected
		? st::dialogsTextPaletteOver
		: st::dialogsTextPalette);

	auto rect = geometry;
	if (!_senderCache.isEmpty()) {
		_senderCache.draw(p, {
			.position = rect.topLeft(),
			.availableWidth = rect.width(),
			.palette = palette,
			.elisionLines = rect.height() / st::dialogsTextFont->height,
		});
		const auto skip = st::dialogsMiniPreviewSkip
			+ st::dialogsMiniPreviewRight;
		rect.setLeft(rect.x() + _senderCache.maxWidth() + skip);
	}
	for (const auto &image : _imagesCache) {
		if (rect.width() < st::dialogsMiniPreview) {
			break;
		}
		p.drawImage(
			rect.x(),
			rect.y() + st::dialogsMiniPreviewTop,
			image.data);
		rect.setLeft(rect.x()
			+ st::dialogsMiniPreview
			+ st::dialogsMiniPreviewSkip);
	}
	if (!_imagesCache.empty()) {
		rect.setLeft(rect.x() + st::dialogsMiniPreviewRight);
	}
	if (rect.isEmpty()) {
		return;
	}
	_textCache.draw(p, {
		.position = rect.topLeft(),
		.availableWidth = rect.width(),
		.palette = palette,
		.spoiler = Text::DefaultSpoilerCache(),
		.now = now,
		.paused = paused,
		.elisionLines = rect.height() / st::dialogsTextFont->height,
	});
}

HistoryView::ItemPreview PreviewWithSender(
		HistoryView::ItemPreview &&preview,
		const TextWithEntities &sender) {
	const auto textWithOffset = tr::lng_dialogs_text_with_from(
		tr::now,
		lt_from_part,
		sender.text,
		lt_message,
		preview.text.text,
		TextWithTagOffset<lt_from_part>::FromString);
	preview.text = tr::lng_dialogs_text_with_from(
		tr::now,
		lt_from_part,
		sender,
		lt_message,
		std::move(preview.text),
		Ui::Text::WithEntities);
	preview.imagesInTextPosition = (textWithOffset.offset < 0)
		? 0
		: textWithOffset.offset + sender.text.size();
	return std::move(preview);
}

} // namespace Dialogs::Ui
