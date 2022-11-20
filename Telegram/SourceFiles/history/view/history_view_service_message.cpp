/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_service_message.h"

#include "history/history.h"
#include "history/history_service.h"
#include "history/view/media/history_view_media.h"
#include "history/history_item_components.h"
#include "history/view/history_view_cursor_state.h"
#include "data/data_abstract_structure.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "ui/chat/chat_style.h"
#include "ui/text/text_options.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "mainwidget.h"
#include "menu/menu_ttl_validator.h"
#include "lang/lang_keys.h"
#include "styles/style_chat.h"

namespace HistoryView {
namespace {

enum CircleMask {
	NormalMask     = 0x00,
	InvertedMask   = 0x01,
};
enum CircleMaskMultiplier {
	MaskMultiplier = 0x04,
};
enum CornerVerticalSide {
	CornerTop      = 0x00,
	CornerBottom   = 0x02,
};
enum CornerHorizontalSide {
	CornerLeft     = 0x00,
	CornerRight    = 0x01,
};

enum class SideStyle {
	Rounded,
	Plain,
	Inverted,
};

// Returns amount of pixels already painted vertically (so you can skip them in the complex rect shape).
int PaintBubbleSide(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		int x,
		int y,
		int width,
		SideStyle style,
		CornerVerticalSide side) {
	if (style == SideStyle::Rounded) {
		const auto &corners = st->serviceBgCornersNormal();
		const auto left = corners.p[(side == CornerTop) ? 0 : 2];
		const auto leftWidth = left.width() / cIntRetinaFactor();
		p.drawPixmap(x, y, left);

		const auto right = corners.p[(side == CornerTop) ? 1 : 3];
		const auto rightWidth = right.width() / cIntRetinaFactor();
		p.drawPixmap(x + width - rightWidth, y, right);

		const auto cornerHeight = left.height() / cIntRetinaFactor();
		p.fillRect(
			x + leftWidth,
			y,
			width - leftWidth - rightWidth,
			cornerHeight,
			st->msgServiceBg());
		return cornerHeight;
	} else if (style == SideStyle::Inverted) {
		// CornerLeft and CornerRight are inverted in the top part.
		const auto &corners = st->serviceBgCornersInverted();
		const auto left = corners.p[(side == CornerTop) ? 1 : 2];
		const auto leftWidth = left.width() / cIntRetinaFactor();
		p.drawPixmap(x - leftWidth, y, left);

		const auto right = corners.p[(side == CornerTop) ? 0 : 3];
		p.drawPixmap(x + width, y, right);
	}
	return 0;
}

void PaintBubblePart(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		int x,
		int y,
		int width,
		int height,
		SideStyle topStyle,
		SideStyle bottomStyle,
		bool forceShrink = false) {
	if ((topStyle == SideStyle::Inverted)
		|| (bottomStyle == SideStyle::Inverted)
		|| forceShrink) {
		width -= Ui::HistoryServiceMsgInvertedShrink() * 2;
		x += Ui::HistoryServiceMsgInvertedShrink();
	}

	if (int skip = PaintBubbleSide(p, st, x, y, width, topStyle, CornerTop)) {
		y += skip;
		height -= skip;
	}
	int bottomSize = 0;
	if (bottomStyle == SideStyle::Rounded) {
		bottomSize = Ui::HistoryServiceMsgRadius();
	} else if (bottomStyle == SideStyle::Inverted) {
		bottomSize = Ui::HistoryServiceMsgInvertedRadius();
	}
	const auto skip = PaintBubbleSide(
		p,
		st,
		x,
		y + height - bottomSize,
		width,
		bottomStyle,
		CornerBottom);
	if (skip) {
		height -= skip;
	}

	p.fillRect(x, y, width, height, st->msgServiceBg());
}

void PaintPreparedDate(
		Painter &p,
		const style::color &bg,
		const Ui::CornersPixmaps &corners,
		const style::color &fg,
		const QString &dateText,
		int dateTextWidth,
		int y,
		int w,
		bool chatWide) {
	int left = st::msgServiceMargin.left();
	const auto maxwidth = chatWide
		? std::min(w, WideChatWidth())
		: w;
	w = maxwidth - st::msgServiceMargin.left() - st::msgServiceMargin.left();

	left += (w - dateTextWidth - st::msgServicePadding.left() - st::msgServicePadding.right()) / 2;
	int height = st::msgServicePadding.top() + st::msgServiceFont->height + st::msgServicePadding.bottom();
	ServiceMessagePainter::PaintBubble(
		p,
		bg,
		corners,
		QRect(
			left,
			y + st::msgServiceMargin.top(),
			dateTextWidth
				+ st::msgServicePadding.left()
				+ st::msgServicePadding.left(),
			height));

	p.setFont(st::msgServiceFont);
	p.setPen(fg);
	p.drawText(
		left + st::msgServicePadding.left(),
		(y
			+ st::msgServiceMargin.top()
			+ st::msgServicePadding.top()
			+ st::msgServiceFont->ascent),
		dateText);
}

bool NeedAboutGroup(not_null<History*> history) {
	if (const auto chat = history->peer->asChat()) {
		return chat->amCreator();
	} else if (const auto channel = history->peer->asMegagroup()) {
		return channel->amCreator();
	}
	return false;
}

} // namepsace

int WideChatWidth() {
	return st::msgMaxWidth + 2 * st::msgPhotoSkip + 2 * st::msgMargin.left();
}

void ServiceMessagePainter::PaintDate(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		const QDateTime &date,
		int y,
		int w,
		bool chatWide) {
	PaintDate(
		p,
		st,
		langDayOfMonthFull(date.date()),
		y,
		w,
		chatWide);
}

void ServiceMessagePainter::PaintDate(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		const QString &dateText,
		int y,
		int w,
		bool chatWide) {
	PaintDate(
		p,
		st,
		dateText,
		st::msgServiceFont->width(dateText),
		y,
		w,
		chatWide);
}

void ServiceMessagePainter::PaintDate(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		const QString &dateText,
		int dateTextWidth,
		int y,
		int w,
		bool chatWide) {
	PaintPreparedDate(
		p,
		st->msgServiceBg(),
		st->serviceBgCornersNormal(),
		st->msgServiceFg(),
		dateText,
		dateTextWidth,
		y,
		w,
		chatWide);
}

void ServiceMessagePainter::PaintDate(
		Painter &p,
		const style::color &bg,
		const Ui::CornersPixmaps &corners,
		const style::color &fg,
		const QString &dateText,
		int dateTextWidth,
		int y,
		int w,
		bool chatWide) {
	PaintPreparedDate(
		p,
		bg,
		corners,
		fg,
		dateText,
		dateTextWidth,
		y,
		w,
		chatWide);
}

void ServiceMessagePainter::PaintBubble(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		QRect rect) {
	PaintBubble(p, st->msgServiceBg(), st->serviceBgCornersNormal(), rect);
}

void ServiceMessagePainter::PaintBubble(
		Painter &p,
		const style::color &bg,
		const Ui::CornersPixmaps &corners,
		QRect rect) {
	Ui::FillRoundRect(p, rect, bg, corners);
}

void ServiceMessagePainter::PaintComplexBubble(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		int left,
		int width,
		const Ui::Text::String &text,
		const QRect &textRect) {
	const auto lineWidths = CountLineWidths(text, textRect);

	int y = st::msgServiceMargin.top(), previousRichWidth = 0;
	bool previousShrink = false, forceShrink = false;
	SideStyle topStyle = SideStyle::Rounded, bottomStyle;
	for (int i = 0, count = lineWidths.size(); i < count; ++i) {
		const auto lineWidth = lineWidths[i];
		if (i + 1 < count) {
			const auto nextLineWidth = lineWidths[i + 1];
			if (nextLineWidth > lineWidth) {
				bottomStyle = SideStyle::Inverted;
			} else if (nextLineWidth < lineWidth) {
				bottomStyle = SideStyle::Rounded;
			} else {
				bottomStyle = SideStyle::Plain;
			}
		} else {
			bottomStyle = SideStyle::Rounded;
		}

		auto richWidth = lineWidth + st::msgServicePadding.left() + st::msgServicePadding.right();
		auto richHeight = st::msgServiceFont->height;
		if (topStyle == SideStyle::Rounded) {
			richHeight += st::msgServicePadding.top();
		} else if (topStyle == SideStyle::Inverted) {
			richHeight -= st::msgServicePadding.bottom();
		}
		if (bottomStyle == SideStyle::Rounded) {
			richHeight += st::msgServicePadding.bottom();
		} else if (bottomStyle == SideStyle::Inverted) {
			richHeight -= st::msgServicePadding.top();
		}
		forceShrink = previousShrink && (richWidth == previousRichWidth);
		PaintBubblePart(
			p,
			st,
			left + ((width - richWidth) / 2),
			y,
			richWidth,
			richHeight,
			topStyle,
			bottomStyle,
			forceShrink);
		y += richHeight;

		previousShrink = forceShrink || (topStyle == SideStyle::Inverted) || (bottomStyle == SideStyle::Inverted);
		previousRichWidth = richWidth;

		if (bottomStyle == SideStyle::Inverted) {
			topStyle = SideStyle::Rounded;
		} else if (bottomStyle == SideStyle::Rounded) {
			topStyle = SideStyle::Inverted;
		} else {
			topStyle = SideStyle::Plain;
		}
	}
}

QVector<int> ServiceMessagePainter::CountLineWidths(
		const Ui::Text::String &text,
		const QRect &textRect) {
	const auto linesCount = qMax(
		textRect.height() / st::msgServiceFont->height,
		1);
	auto result = QVector<int>();
	result.reserve(linesCount);
	text.countLineWidths(textRect.width(), &result);

	const auto minDelta = 2 * (Ui::HistoryServiceMsgRadius()
		+ Ui::HistoryServiceMsgInvertedRadius()
		- Ui::HistoryServiceMsgInvertedShrink());
	for (int i = 0, count = result.size(); i != count; ++i) {
		auto width = qMax(result[i], 0);
		if (i > 0) {
			const auto widthBefore = result[i - 1];
			if (width < widthBefore && width + minDelta > widthBefore) {
				width = widthBefore;
			}
		}
		if (i + 1 < count) {
			const auto widthAfter = result[i + 1];
			if (width < widthAfter && width + minDelta > widthAfter) {
				width = widthAfter;
			}
		}
		if (width > result[i]) {
			result[i] = width;
			if (i > 0) {
				int widthBefore = result[i - 1];
				if (widthBefore != width
					&& widthBefore < width + minDelta
					&& widthBefore + minDelta > width) {
					i -= 2;
				}
			}
		}
	}
	return result;
}

Service::Service(
	not_null<ElementDelegate*> delegate,
	not_null<HistoryService*> data,
	Element *replacing)
: Element(delegate, data, replacing, Flag::ServiceMessage) {
}

not_null<HistoryService*> Service::message() const {
	return static_cast<HistoryService*>(data().get());
}

QRect Service::innerGeometry() const {
	return countGeometry();
}

QRect Service::countGeometry() const {
	auto result = QRect(0, 0, width(), height());
	if (delegate()->elementIsChatWide()) {
		result.setWidth(qMin(result.width(), st::msgMaxWidth + 2 * st::msgPhotoSkip + 2 * st::msgMargin.left()));
	}
	return result.marginsRemoved(st::msgServiceMargin);
}

QSize Service::performCountCurrentSize(int newWidth) {
	auto newHeight = displayedDateHeight();
	if (const auto bar = Get<UnreadBar>()) {
		newHeight += bar->height();
	}

	if (isHidden()) {
		return { newWidth, newHeight };
	}

	if (!text().isEmpty()) {
		auto contentWidth = newWidth;
		if (delegate()->elementIsChatWide()) {
			accumulate_min(contentWidth, st::msgMaxWidth + 2 * st::msgPhotoSkip + 2 * st::msgMargin.left());
		}
		contentWidth -= st::msgServiceMargin.left() + st::msgServiceMargin.left(); // two small margins
		if (contentWidth < st::msgServicePadding.left() + st::msgServicePadding.right() + 1) {
			contentWidth = st::msgServicePadding.left() + st::msgServicePadding.right() + 1;
		}

		auto nwidth = qMax(contentWidth - st::msgServicePadding.left() - st::msgServicePadding.right(), 0);
		newHeight += (contentWidth >= maxWidth())
			? minHeight()
			: textHeightFor(nwidth);
		newHeight += st::msgServicePadding.top() + st::msgServicePadding.bottom() + st::msgServiceMargin.top() + st::msgServiceMargin.bottom();
		if (const auto media = this->media()) {
			newHeight += st::msgServiceMargin.top() + media->resizeGetHeight(media->maxWidth());
		}
	}

	return { newWidth, newHeight };
}

QSize Service::performCountOptimalSize() {
	validateText();

	auto maxWidth = text().maxWidth() + st::msgServicePadding.left() + st::msgServicePadding.right();
	auto minHeight = text().minHeight();
	if (const auto media = this->media()) {
		media->initDimensions();
	}
	return { maxWidth, minHeight };
}

bool Service::isHidden() const {
	return Element::isHidden();
}

int Service::marginTop() const {
	return st::msgServiceMargin.top();
}

int Service::marginBottom() const {
	return st::msgServiceMargin.bottom();
}

void Service::draw(Painter &p, const PaintContext &context) const {
	auto g = countGeometry();
	if (g.width() < 1) {
		return;
	}
	const auto &margin = st::msgServiceMargin;

	const auto st = context.st;
	auto height = this->height() - margin.top() - margin.bottom();
	auto dateh = 0;
	auto unreadbarh = 0;
	auto clip = context.clip;
	if (auto date = Get<DateBadge>()) {
		dateh = date->height();
		p.translate(0, dateh);
		clip.translate(0, -dateh);
		height -= dateh;
	}
	if (const auto bar = Get<UnreadBar>()) {
		unreadbarh = bar->height();
		if (clip.intersects(QRect(0, 0, width(), unreadbarh))) {
			bar->paint(
				p,
				context,
				0,
				width(),
				delegate()->elementIsChatWide());
		}
		p.translate(0, unreadbarh);
		clip.translate(0, -unreadbarh);
		height -= unreadbarh;
	}

	if (isHidden()) {
		if (auto skiph = dateh + unreadbarh) {
			p.translate(0, -skiph);
		}
		return;
	}

	paintHighlight(p, context, height);

	p.setTextPalette(st->serviceTextPalette());

	const auto media = this->media();
	if (media) {
		height -= margin.top() + media->height();
	}

	const auto trect = QRect(g.left(), margin.top(), g.width(), height)
		- st::msgServicePadding;

	ServiceMessagePainter::PaintComplexBubble(
		p,
		context.st,
		g.left(),
		g.width(),
		text(),
		trect);

	p.setBrush(Qt::NoBrush);
	p.setPen(st->msgServiceFg());
	p.setFont(st::msgServiceFont);
	prepareCustomEmojiPaint(p, context, text());
	text().draw(p, {
		.position = trect.topLeft(),
		.availableWidth = trect.width(),
		.align = style::al_top,
		.palette = &st->serviceTextPalette(),
		.spoiler = Ui::Text::DefaultSpoilerCache(),
		.now = context.now,
		.paused = context.paused,
		.selection = context.selection,
		.fullWidthSelection = false,
	});

	if (media) {
		const auto left = margin.left() + (g.width() - media->maxWidth()) / 2;
		const auto top = margin.top() + height + margin.top();
		p.translate(left, top);
		media->draw(p, context.translated(-left, -top).withSelection({}));
		p.translate(-left, -top);
	}

	if (auto skiph = dateh + unreadbarh) {
		p.translate(0, -skiph);
	}
}

PointState Service::pointState(QPoint point) const {
	const auto media = this->media();

	auto g = countGeometry();
	if (g.width() < 1 || isHidden()) {
		return PointState::Outside;
	}

	if (const auto dateh = displayedDateHeight()) {
		g.setTop(g.top() + dateh);
	}
	if (const auto bar = Get<UnreadBar>()) {
		g.setTop(g.top() + bar->height());
	}
	if (media) {
		const auto centerPadding = (g.width() - media->width()) / 2;
		const auto r = g - QMargins(centerPadding, 0, centerPadding, 0);
		if (!r.contains(point)) {
			g.setHeight(g.height()
				- (st::msgServiceMargin.top() + media->height()));
		}
	}
	return g.contains(point) ? PointState::Inside : PointState::Outside;
}

TextState Service::textState(QPoint point, StateRequest request) const {
	const auto item = message();
	const auto media = this->media();

	auto result = TextState(item);

	auto g = countGeometry();
	if (g.width() < 1 || isHidden()) {
		return result;
	}

	if (const auto dateh = displayedDateHeight()) {
		point.setY(point.y() - dateh);
		g.setHeight(g.height() - dateh);
	}
	if (const auto bar = Get<UnreadBar>()) {
		auto unreadbarh = bar->height();
		point.setY(point.y() - unreadbarh);
		g.setHeight(g.height() - unreadbarh);
	}

	if (media) {
		g.setHeight(g.height() - (st::msgServiceMargin.top() + media->height()));
	}
	auto trect = g.marginsAdded(-st::msgServicePadding);
	if (trect.contains(point)) {
		auto textRequest = request.forText();
		textRequest.align = style::al_center;
		result = TextState(item, text().getState(
			point - trect.topLeft(),
			trect.width(),
			textRequest));
		if (!result.link
			&& result.cursor == CursorState::Text
			&& g.contains(point)) {
			if (const auto gamescore = item->Get<HistoryServiceGameScore>()) {
				result.link = gamescore->lnk;
			} else if (const auto payment = item->Get<HistoryServicePayment>()) {
				result.link = payment->invoiceLink;
			} else if (const auto call = item->Get<HistoryServiceOngoingCall>()) {
				const auto peer = history()->peer;
				if (PeerHasThisCall(peer, call->id).value_or(false)) {
					result.link = call->link;
				}
			} else if (const auto theme = item->Get<HistoryServiceChatThemeChange>()) {
				result.link = theme->link;
			} else if (const auto ttl = item->Get<HistoryServiceTTLChange>()) {
				if (TTLMenu::TTLValidator(nullptr, history()->peer).can()) {
					result.link = ttl->link;
				}
			}
		}
	} else if (media) {
		result = media->textState(point - QPoint(st::msgServiceMargin.left() + (g.width() - media->maxWidth()) / 2, st::msgServiceMargin.top() + g.height() + st::msgServiceMargin.top()), request);
	}
	return result;
}

void Service::updatePressed(QPoint point) {
}

TextForMimeData Service::selectedText(TextSelection selection) const {
	return text().toTextForMimeData(selection);
}

TextSelection Service::adjustSelection(
		TextSelection selection,
		TextSelectType type) const {
	return text().adjustSelection(selection, type);
}

EmptyPainter::EmptyPainter(not_null<History*> history) : _history(history) {
	if (NeedAboutGroup(_history)) {
		fillAboutGroup();
	}
}

void EmptyPainter::fillAboutGroup() {
	const auto phrases = {
		tr::lng_group_about1(tr::now),
		tr::lng_group_about2(tr::now),
		tr::lng_group_about3(tr::now),
		tr::lng_group_about4(tr::now),
	};
	const auto setText = [](Ui::Text::String &text, const QString &content) {
		text.setText(
			st::serviceTextStyle,
			content,
			Ui::NameTextOptions());
	};
	setText(_header, tr::lng_group_about_header(tr::now));
	setText(_text, tr::lng_group_about_text(tr::now));
	for (const auto &text : phrases) {
		_phrases.emplace_back(st::msgMinWidth);
		setText(_phrases.back(), text);
	}
}

void EmptyPainter::paint(
		Painter &p,
		not_null<const Ui::ChatStyle*> st,
		int width,
		int height) {
	if (_phrases.empty()) {
		return;
	}
	constexpr auto kMaxTextLines = 3;
	const auto maxPhraseWidth = ranges::max_element(
		_phrases,
		ranges::less(),
		&Ui::Text::String::maxWidth
	)->maxWidth();

	const auto &font = st::serviceTextStyle.font;
	const auto maxBubbleWidth = width - 2 * st::historyGroupAboutMargin;
	const auto padding = st::historyGroupAboutPadding;
	const auto bubbleWidth = std::min(
		maxBubbleWidth,
		std::max({
			maxPhraseWidth + st::historyGroupAboutBulletSkip,
			_header.maxWidth(),
			_text.maxWidth() }) + padding.left() + padding.right());
	const auto innerWidth = bubbleWidth - padding.left() - padding.right();
	const auto textHeight = [&](const Ui::Text::String &text) {
		return std::min(
			text.countHeight(innerWidth),
			kMaxTextLines * font->height);
	};
	const auto bubbleHeight = padding.top()
		+ textHeight(_header)
		+ st::historyGroupAboutHeaderSkip
		+ textHeight(_text)
		+ st::historyGroupAboutTextSkip
		+ ranges::accumulate(_phrases, 0, ranges::plus(), textHeight)
		+ st::historyGroupAboutSkip * int(_phrases.size() - 1)
		+ padding.bottom();
	const auto bubbleLeft = (width - bubbleWidth) / 2;
	const auto bubbleTop = (height - bubbleHeight) / 2;

	ServiceMessagePainter::PaintBubble(
		p,
		st->msgServiceBg(),
		st->serviceBgCornersNormal(),
		QRect(bubbleLeft, bubbleTop, bubbleWidth, bubbleHeight));

	p.setPen(st->msgServiceFg());
	p.setBrush(st->msgServiceFg());

	const auto left = bubbleLeft + padding.left();
	auto top = bubbleTop + padding.top();

	_header.drawElided(
		p,
		left,
		top,
		innerWidth,
		kMaxTextLines,
		style::al_top);
	top += textHeight(_header) + st::historyGroupAboutHeaderSkip;

	_text.drawElided(
		p,
		left,
		top,
		innerWidth,
		kMaxTextLines);
	top += textHeight(_text) + st::historyGroupAboutTextSkip;

	for (const auto &text : _phrases) {
		p.setPen(st->msgServiceFg());
		text.drawElided(
			p,
			left + st::historyGroupAboutBulletSkip,
			top,
			innerWidth,
			kMaxTextLines);

		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.drawEllipse(
			left,
			top + (font->height - st::mediaUnreadSize) / 2,
			st::mediaUnreadSize,
			st::mediaUnreadSize);
		top += textHeight(text) + st::historyGroupAboutSkip;
	}
}

} // namespace HistoryView
