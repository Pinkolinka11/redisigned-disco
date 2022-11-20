/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "platform/linux/linux_xdp_open_with_dialog.h"

#include "base/platform/base_platform_info.h"
#include "base/platform/linux/base_linux_glibmm_helper.h"
#include "base/platform/linux/base_linux_xdp_utilities.h"
#include "base/platform/linux/base_linux_wayland_integration.h"
#include "core/application.h"
#include "window/window_controller.h"
#include "base/random.h"

#include <fcntl.h>
#include <glibmm.h>
#include <giomm.h>

namespace Platform {
namespace File {
namespace internal {
namespace {

constexpr auto kXDPOpenURIInterface = "org.freedesktop.portal.OpenURI"_cs;
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties"_cs;

} // namespace

bool ShowXDPOpenWithDialog(const QString &filepath) {
	try {
		const auto connection = Gio::DBus::Connection::get_sync(
			Gio::DBus::BusType::BUS_TYPE_SESSION);

		auto reply = connection->call_sync(
			std::string(base::Platform::XDP::kObjectPath),
			std::string(kPropertiesInterface),
			"Get",
			base::Platform::MakeGlibVariant(std::tuple{
				Glib::ustring(
					std::string(kXDPOpenURIInterface)),
				Glib::ustring("version"),
			}),
			std::string(base::Platform::XDP::kService));

		const auto version = base::Platform::GlibVariantCast<uint>(
			base::Platform::GlibVariantCast<Glib::VariantBase>(
				reply.get_child(0)));

		if (version < 3) {
			return false;
		}

		const auto filepathUtf8 = filepath.toUtf8();

		const auto fd = open(
			filepathUtf8.constData(),
			O_RDONLY);

		if (fd == -1) {
			return false;
		}

		const auto fdGuard = gsl::finally([&] { ::close(fd); });

		const auto parentWindowId = [&]() -> Glib::ustring {
			const auto activeWindow = Core::App().activeWindow();
			if (!activeWindow) {
				return {};
			}

			return base::Platform::XDP::ParentWindowID(
				activeWindow->widget()->windowHandle());
		}();

		const auto handleToken = Glib::ustring("tdesktop")
			+ std::to_string(base::RandomValue<uint>());

		const auto activationToken = []() -> Glib::ustring {
			using base::Platform::WaylandIntegration;
			if (const auto integration = WaylandIntegration::Instance()) {
				if (const auto token = integration->activationToken()
					; !token.isNull()) {
					return token.toStdString();
				}
			}
			return {};
		}();

		auto uniqueName = connection->get_unique_name();
		uniqueName.erase(0, 1);
		uniqueName.replace(uniqueName.find('.'), 1, 1, '_');

		const auto requestPath = Glib::ustring(
				"/org/freedesktop/portal/desktop/request/")
			+ uniqueName
			+ '/'
			+ handleToken;

		const auto loop = Glib::MainLoop::create();

		const auto signalId = connection->signal_subscribe(
			[&](
				const Glib::RefPtr<Gio::DBus::Connection> &connection,
				const Glib::ustring &sender_name,
				const Glib::ustring &object_path,
				const Glib::ustring &interface_name,
				const Glib::ustring &signal_name,
				const Glib::VariantContainerBase &parameters) {
				loop->quit();
			},
			std::string(base::Platform::XDP::kService),
			"org.freedesktop.portal.Request",
			"Response",
			requestPath);

		const auto signalGuard = gsl::finally([&] {
			if (signalId != 0) {
				connection->signal_unsubscribe(signalId);
			}
		});

		const auto fdList = Gio::UnixFDList::create();
		fdList->append(fd);
		auto outFdList = Glib::RefPtr<Gio::UnixFDList>();

		connection->call_sync(
			std::string(base::Platform::XDP::kObjectPath),
			std::string(kXDPOpenURIInterface),
			"OpenFile",
			Glib::VariantContainerBase::create_tuple({
				Glib::Variant<Glib::ustring>::create(parentWindowId),
				Glib::wrap(g_variant_new_handle(0)),
				Glib::Variant<std::map<
					Glib::ustring,
					Glib::VariantBase
				>>::create({
					{
						"handle_token",
						Glib::Variant<Glib::ustring>::create(handleToken)
					},
					{
						"activation_token",
						Glib::Variant<Glib::ustring>::create(activationToken)
					},
					{
						"ask",
						Glib::Variant<bool>::create(true)
					},
				}),
			}),
			fdList,
			outFdList,
			std::string(base::Platform::XDP::kService));

		if (signalId != 0) {
			QWidget window;
			window.setAttribute(Qt::WA_DontShowOnScreen);
			window.setWindowModality(Qt::ApplicationModal);
			window.show();
			loop->run();
		}

		return true;
	} catch (...) {
	}

	return false;
}

} // namespace internal
} // namespace File
} // namespace Platform
