/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "platform/platform_tray.h"

#include "base/unique_qptr.h"

namespace Window {
class CounterLayerArgs;
} // namespace Window

namespace Ui {
class PopupMenu;
} // namespace Ui

class QSystemTrayIcon;

namespace Platform {

class Tray final {
public:
	Tray();

	[[nodiscard]] rpl::producer<> aboutToShowRequests() const;
	[[nodiscard]] rpl::producer<> showFromTrayRequests() const;
	[[nodiscard]] rpl::producer<> hideToTrayRequests() const;
	[[nodiscard]] rpl::producer<> iconClicks() const;

	[[nodiscard]] bool hasIcon() const;

	void createIcon();
	void destroyIcon();

	void updateIcon();

	void createMenu();
	void destroyMenu();

	void addAction(rpl::producer<QString> text, Fn<void()> &&callback);

	void showTrayMessage() const;
	[[nodiscard]] bool hasTrayMessageSupport() const;

	[[nodiscard]] rpl::lifetime &lifetime();

	// Windows only.
	[[nodiscard]] static Window::CounterLayerArgs CounterLayerArgs(
		int size,
		int counter,
		bool muted);
	[[nodiscard]] static QPixmap IconWithCounter(
		Window::CounterLayerArgs &&args,
		bool smallIcon,
		bool supportMode);

private:
	base::unique_qptr<QSystemTrayIcon> _icon;
	base::unique_qptr<Ui::PopupMenu> _menu;

	rpl::event_stream<> _iconClicks;
	rpl::event_stream<> _aboutToShowRequests;

	rpl::lifetime _callbackFromTrayLifetime;
	rpl::lifetime _actionsLifetime;
	rpl::lifetime _lifetime;

};

} // namespace Platform
