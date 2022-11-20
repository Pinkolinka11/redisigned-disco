/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_common.h"

namespace Main {
class Account;
} // namespace Main

namespace Window {
class Controller;
} // namespace Window

namespace Settings {

void SetupConnectionType(
	not_null<Window::Controller*> controller,
	not_null<::Main::Account*> account,
	not_null<Ui::VerticalLayout*> container);
bool HasUpdate();
void SetupUpdate(
	not_null<Ui::VerticalLayout*> container,
	Fn<void(Type)> showOther);
void SetupSystemIntegrationContent(
	Window::SessionController *controller,
	not_null<Ui::VerticalLayout*> container);
void SetupAnimations(not_null<Ui::VerticalLayout*> container);

class Advanced : public Section<Advanced> {
public:
	Advanced(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

	rpl::producer<Type> sectionShowOther() override;

private:
	void setupContent(not_null<Window::SessionController*> controller);

	rpl::event_stream<Type> _showOther;

};

} // namespace Settings
