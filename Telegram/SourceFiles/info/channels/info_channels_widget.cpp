/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/channels/info_channels_widget.h"

#include "info/feed/info_feed_channels.h"
#include "info/info_controller.h"
#include "ui/widgets/scroll_area.h"
#include "styles/style_info.h"

namespace Info {
namespace Channels {

Memento::Memento(not_null<Controller*> controller)
: Memento(controller->feed()) {
}

Memento::Memento(not_null<Data::Feed*> feed)
: ContentMemento(feed) {
}

Section Memento::section() const {
	return Section(Section::Type::Channels);
}

object_ptr<ContentWidget> Memento::createWidget(
		QWidget *parent,
		not_null<Controller*> controller,
		const QRect &geometry) {
	auto result = object_ptr<Widget>(
		parent,
		controller);
	result->setInternalState(geometry, this);
	return result;
}

void Memento::setState(std::unique_ptr<SavedState> state) {
	_state = std::move(state);
}

std::unique_ptr<SavedState> Memento::state() {
	return std::move(_state);
}

Memento::~Memento() = default;

Widget::Widget(
	QWidget *parent,
	not_null<Controller*> controller)
: ContentWidget(parent, controller) {
	_inner = setInnerWidget(object_ptr<FeedProfile::Channels>(
		this,
		controller));
}

bool Widget::showInternal(not_null<ContentMemento*> memento) {
	if (!controller()->validateMementoPeer(memento)) {
		return false;
	}
	if (auto membersMemento = dynamic_cast<Memento*>(memento.get())) {
		restoreState(membersMemento);
		return true;
	}
	return false;
}

void Widget::setInternalState(
		const QRect &geometry,
		not_null<Memento*> memento) {
	setGeometry(geometry);
	Ui::SendPendingMoveResizeEvents(this);
	restoreState(memento);
}

std::unique_ptr<ContentMemento> Widget::doCreateMemento() {
	auto result = std::make_unique<Memento>(controller());
	saveState(result.get());
	return result;
}

void Widget::saveState(not_null<Memento*> memento) {
	memento->setScrollTop(scrollTopSave());
	memento->setState(_inner->saveState());
}

void Widget::restoreState(not_null<Memento*> memento) {
	_inner->restoreState(memento->state());
	auto scrollTop = memento->scrollTop();
	scrollTopRestore(memento->scrollTop());
}

} // namespace Channels
} // namespace Info
