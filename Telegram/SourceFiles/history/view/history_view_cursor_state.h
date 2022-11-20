/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace HistoryView {

class Element;

enum class PointState : char {
	Outside,
	Inside,
	GroupPart,
};
enum class CursorState : char {
	None,
	Text,
	Date,
	Forwarded,
};

struct TextState {
	TextState() = default;
	TextState(not_null<const HistoryItem*> item);
	TextState(
		not_null<const HistoryItem*> item,
		const Ui::Text::StateResult &state);
	TextState(
		not_null<const HistoryItem*> item,
		ClickHandlerPtr link);
	TextState(not_null<const HistoryView::Element*> view);
	TextState(
		not_null<const HistoryView::Element*> view,
		const Ui::Text::StateResult &state);
	TextState(
		not_null<const HistoryView::Element*> view,
		ClickHandlerPtr link);
	TextState(
		std::nullptr_t,
		const Ui::Text::StateResult &state);
	TextState(std::nullptr_t, ClickHandlerPtr link);

	FullMsgId itemId;
	CursorState cursor = CursorState::None;
	ClickHandlerPtr link;
	bool afterSymbol = false;
	bool customTooltip = false;
	uint16 symbol = 0;
	QString customTooltipText;

};

struct StateRequest {
	Ui::Text::StateRequest::Flags flags = Ui::Text::StateRequest::Flag::LookupLink;
	Ui::Text::StateRequest forText() const {
		Ui::Text::StateRequest result;
		result.flags = flags;
		return result;
	}
};

enum class InfoDisplayType : char {
	Default,
	Image,
	Background,
};

} // namespace HistoryView
