/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/edit_privacy_box.h"

#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/text/text_utilities.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "history/history.h"
#include "boxes/peer_list_controllers.h"
#include "settings/settings_common.h"
#include "settings/settings_privacy_security.h"
#include "calls/calls_instance.h"
#include "base/binary_guard.h"
#include "lang/lang_keys.h"
#include "apiwrap.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "window/window_session_controller.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"

namespace {

class PrivacyExceptionsBoxController : public ChatsListBoxController {
public:
	PrivacyExceptionsBoxController(
		not_null<Main::Session*> session,
		rpl::producer<QString> title,
		const std::vector<not_null<PeerData*>> &selected);

	Main::Session &session() const override;
	void rowClicked(not_null<PeerListRow*> row) override;

protected:
	void prepareViewHook() override;
	std::unique_ptr<Row> createRow(not_null<History*> history) override;

private:
	const not_null<Main::Session*> _session;
	rpl::producer<QString> _title;
	std::vector<not_null<PeerData*>> _selected;

};

PrivacyExceptionsBoxController::PrivacyExceptionsBoxController(
	not_null<Main::Session*> session,
	rpl::producer<QString> title,
	const std::vector<not_null<PeerData*>> &selected)
: ChatsListBoxController(session)
, _session(session)
, _title(std::move(title))
, _selected(selected) {
}

Main::Session &PrivacyExceptionsBoxController::session() const {
	return *_session;
}

void PrivacyExceptionsBoxController::prepareViewHook() {
	delegate()->peerListSetTitle(std::move(_title));
	delegate()->peerListAddSelectedPeers(_selected);
}

void PrivacyExceptionsBoxController::rowClicked(not_null<PeerListRow*> row) {
	const auto peer = row->peer();

	// This call may delete row, if it was a search result row.
	delegate()->peerListSetRowChecked(row, !row->checked());

	if (const auto channel = peer->asChannel()) {
		if (!channel->membersCountKnown()) {
			channel->updateFull();
		}
	}
}

std::unique_ptr<PrivacyExceptionsBoxController::Row> PrivacyExceptionsBoxController::createRow(not_null<History*> history) {
	if (history->peer->isSelf() || history->peer->isRepliesChat()) {
		return nullptr;
	} else if (!history->peer->isUser()
		&& !history->peer->isChat()
		&& !history->peer->isMegagroup()) {
		return nullptr;
	}
	auto result = std::make_unique<Row>(history);
	const auto count = [&] {
		if (const auto chat = history->peer->asChat()) {
			return chat->count;
		} else if (const auto channel = history->peer->asChannel()) {
			return channel->membersCountKnown()
				? channel->membersCount()
				: 0;
		}
		return 0;
	}();
	if (count > 0) {
		result->setCustomStatus(
			tr::lng_chat_status_members(tr::now, lt_count_decimal, count));
	}
	return result;
}

} // namespace

QString EditPrivacyController::optionLabel(Option option) const {
	switch (option) {
	case Option::Everyone: return tr::lng_edit_privacy_everyone(tr::now);
	case Option::Contacts: return tr::lng_edit_privacy_contacts(tr::now);
	case Option::Nobody: return tr::lng_edit_privacy_nobody(tr::now);
	}
	Unexpected("Option value in optionsLabelKey.");
}

EditPrivacyBox::EditPrivacyBox(
	QWidget*,
	not_null<Window::SessionController*> window,
	std::unique_ptr<EditPrivacyController> controller,
	const Value &value)
: _window(window)
, _controller(std::move(controller))
, _value(value) {
}

void EditPrivacyBox::prepare() {
	_controller->setView(this);

	setupContent();
}

void EditPrivacyBox::editExceptions(
		Exception exception,
		Fn<void()> done) {
	auto controller = std::make_unique<PrivacyExceptionsBoxController>(
		&_window->session(),
		_controller->exceptionBoxTitle(exception),
		exceptions(exception));
	auto initBox = [=, controller = controller.get()](
			not_null<PeerListBox*> box) {
		box->addButton(tr::lng_settings_save(), crl::guard(this, [=] {
			exceptions(exception) = box->collectSelectedRows();
			const auto type = [&] {
				switch (exception) {
				case Exception::Always: return Exception::Never;
				case Exception::Never: return Exception::Always;
				}
				Unexpected("Invalid exception value.");
			}();
			auto &removeFrom = exceptions(type);
			for (const auto peer : exceptions(exception)) {
				removeFrom.erase(
					ranges::remove(removeFrom, peer),
					end(removeFrom));
			}
			done();
			box->closeBox();
		}));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	_window->show(
		Box<PeerListBox>(std::move(controller), std::move(initBox)),
		Ui::LayerOption::KeepOther);
}

std::vector<not_null<PeerData*>> &EditPrivacyBox::exceptions(Exception exception) {
	switch (exception) {
	case Exception::Always: return _value.always;
	case Exception::Never: return _value.never;
	}
	Unexpected("Invalid exception value.");
}

bool EditPrivacyBox::showExceptionLink(Exception exception) const {
	switch (exception) {
	case Exception::Always:
		return (_value.option == Option::Contacts)
			|| (_value.option == Option::Nobody);
	case Exception::Never:
		return (_value.option == Option::Everyone)
			|| (_value.option == Option::Contacts);
	}
	Unexpected("Invalid exception value.");
}

Ui::Radioenum<EditPrivacyBox::Option> *EditPrivacyBox::AddOption(
		not_null<Ui::VerticalLayout*> container,
		not_null<EditPrivacyController*> controller,
		const std::shared_ptr<Ui::RadioenumGroup<Option>> &group,
		Option option) {
	return container->add(
		object_ptr<Ui::Radioenum<Option>>(
			container,
			group,
			option,
			controller->optionLabel(option),
			st::settingsPrivacyOption),
		(st::settingsSendTypePadding + style::margins(
			-st::lineWidth,
			st::settingsPrivacySkipTop,
			0,
			0)));
}

Ui::FlatLabel *EditPrivacyBox::addLabel(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<TextWithEntities> text,
		int topSkip) {
	if (!text) {
		return nullptr;
	}
	return container->add(
		object_ptr<Ui::DividerLabel>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				rpl::duplicate(text),
				st::boxDividerLabel),
			st::settingsDividerLabelPadding),
		{ 0, topSkip, 0, 0 }
	)->entity();
}

Ui::FlatLabel *EditPrivacyBox::addLabelOrDivider(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<TextWithEntities> text,
		int topSkip) {
	if (const auto result = addLabel(container, std::move(text), topSkip)) {
		return result;
	}
	container->add(
		object_ptr<Ui::BoxContentDivider>(container),
		{ 0, topSkip, 0, 0 });
	return nullptr;
}

void EditPrivacyBox::setupContent() {
	using namespace Settings;

	setTitle(_controller->title());

	auto wrap = object_ptr<Ui::VerticalLayout>(this);
	const auto content = wrap.data();
	setInnerWidget(object_ptr<Ui::OverrideMargins>(
		this,
		std::move(wrap)));

	const auto group = std::make_shared<Ui::RadioenumGroup<Option>>(
		_value.option);
	const auto toggle = Ui::CreateChild<rpl::event_stream<Option>>(content);
	group->setChangedCallback([=](Option value) {
		_value.option = value;
		toggle->fire_copy(value);
	});
	auto optionValue = toggle->events_starting_with_copy(_value.option);

	const auto addOptionRow = [&](Option option) {
		return (_controller->hasOption(option) || (_value.option == option))
			? AddOption(content, _controller.get(), group, option)
			: nullptr;
	};
	const auto addExceptionLink = [=](Exception exception) {
		const auto update = Ui::CreateChild<rpl::event_stream<>>(content);
		auto label = update->events_starting_with({}) | rpl::map([=] {
			return Settings::ExceptionUsersCount(exceptions(exception));
		}) | rpl::map([](int count) {
			return count
				? tr::lng_edit_privacy_exceptions_count(tr::now, lt_count, count)
				: tr::lng_edit_privacy_exceptions_add(tr::now);
		});
		auto text = _controller->exceptionButtonTextKey(exception);
		const auto always = (exception == Exception::Always);
		const auto button = content->add(
			object_ptr<Ui::SlideWrap<Button>>(
				content,
				CreateButton(
					content,
					rpl::duplicate(text),
					st::settingsButton,
					{
						(always
							? &st::settingsIconPlus
							: &st::settingsIconMinus),
						always ? kIconGreen : kIconRed,
					})));
		CreateRightLabel(
			button->entity(),
			std::move(label),
			st::settingsButton,
			std::move(text));
		button->toggleOn(rpl::duplicate(
			optionValue
		) | rpl::map([=] {
			return showExceptionLink(exception);
		}))->entity()->addClickHandler([=] {
			editExceptions(exception, [=] { update->fire({}); });
		});
		return button;
	};

	auto above = _controller->setupAboveWidget(
		content,
		rpl::duplicate(optionValue),
		getDelegate()->outerContainer());
	if (above) {
		content->add(std::move(above));
	}

	AddSubsectionTitle(
		content,
		_controller->optionsTitleKey(),
		{ 0, st::settingsPrivacySkipTop, 0, 0 });
	addOptionRow(Option::Everyone);
	addOptionRow(Option::Contacts);
	addOptionRow(Option::Nobody);
	const auto warning = addLabelOrDivider(
		content,
		_controller->warning(),
		st::settingsSectionSkip + st::settingsPrivacySkipTop);
	if (warning) {
		_controller->prepareWarningLabel(warning);
	}

	auto middle = _controller->setupMiddleWidget(
		_window,
		content,
		std::move(optionValue));
	if (middle) {
		content->add(std::move(middle));
	}

	AddSkip(content);
	AddSubsectionTitle(
		content,
		tr::lng_edit_privacy_exceptions(),
		{ 0, st::settingsPrivacySkipTop, 0, 0 });
	const auto always = addExceptionLink(Exception::Always);
	const auto never = addExceptionLink(Exception::Never);
	addLabel(
		content,
		_controller->exceptionsDescription() | Ui::Text::ToWithEntities(),
		st::settingsSectionSkip);

	if (auto below = _controller->setupBelowWidget(_window, content)) {
		content->add(std::move(below));
	}

	addButton(tr::lng_settings_save(), [=] {
		const auto someAreDisallowed = (_value.option != Option::Everyone)
			|| !_value.never.empty();
		_controller->confirmSave(someAreDisallowed, crl::guard(this, [=] {
			_value.ignoreAlways = !showExceptionLink(Exception::Always);
			_value.ignoreNever = !showExceptionLink(Exception::Never);

			_controller->saveAdditional();
			_window->session().api().userPrivacy().save(
				_controller->key(),
				_value);
			closeBox();
		}));
	});
	addButton(tr::lng_cancel(), [this] { closeBox(); });

	const auto linkHeight = st::settingsButton.padding.top()
		+ st::settingsButton.height
		+ st::settingsButton.padding.bottom();

	widthValue(
	) | rpl::start_with_next([=](int width) {
		content->resizeToWidth(width);
	}, content->lifetime());

	content->heightValue(
	) | rpl::map([=](int height) {
		return height - always->height() - never->height() + 2 * linkHeight;
	}) | rpl::distinct_until_changed(
	) | rpl::start_with_next([=](int height) {
		setDimensions(st::boxWideWidth, height);
	}, content->lifetime());
}
