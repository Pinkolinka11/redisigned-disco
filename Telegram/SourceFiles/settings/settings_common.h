/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/round_rect.h"
#include "base/object_ptr.h"
#include "settings/settings_type.h"

namespace anim {
enum class repeat : uchar;
} // namespace anim

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class VerticalLayout;
class FlatLabel;
class SettingsButton;
class AbstractButton;
} // namespace Ui

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace Window {
class SessionController;
} // namespace Window

namespace style {
struct FlatLabel;
struct SettingsButton;
} // namespace style

namespace Lottie {
struct IconDescriptor;
} // namespace Lottie

namespace Settings {

extern const char kOptionMonoSettingsIcons[];

using Button = Ui::SettingsButton;

class AbstractSection;

struct SectionMeta {
	[[nodiscard]] virtual object_ptr<AbstractSection> create(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller) const = 0;
};

template <typename SectionType>
struct SectionMetaImplementation : SectionMeta {
	object_ptr<AbstractSection> create(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller
	) const final override {
		return object_ptr<SectionType>(parent, controller);
	}

	[[nodiscard]] static not_null<SectionMeta*> Meta() {
		static SectionMetaImplementation result;
		return &result;
	}
};

class AbstractSection : public Ui::RpWidget {
public:
	using RpWidget::RpWidget;

	[[nodiscard]] virtual Type id() const = 0;
	[[nodiscard]] virtual rpl::producer<Type> sectionShowOther() {
		return nullptr;
	}
	[[nodiscard]] virtual rpl::producer<> sectionShowBack() {
		return nullptr;
	}
	[[nodiscard]] virtual rpl::producer<std::vector<Type>> removeFromStack() {
		return nullptr;
	}
	[[nodiscard]] virtual rpl::producer<QString> title() = 0;
	virtual void sectionSaveChanges(FnMut<void()> done) {
		done();
	}
	virtual void showFinished() {
	}
	virtual void setInnerFocus() {
		setFocus();
	}
	[[nodiscard]] virtual const Ui::RoundRect *bottomSkipRounding() const {
		return nullptr;
	}
	[[nodiscard]] virtual QPointer<Ui::RpWidget> createPinnedToTop(
			not_null<QWidget*> parent) {
		return nullptr;
	}
	[[nodiscard]] virtual QPointer<Ui::RpWidget> createPinnedToBottom(
			not_null<Ui::RpWidget*> parent) {
		return nullptr;
	}
	[[nodiscard]] virtual bool hasFlexibleTopBar() const {
		return false;
	}
	virtual void setStepDataReference(std::any &data) {
	}
};

template <typename SectionType>
class Section : public AbstractSection {
public:
	using AbstractSection::AbstractSection;

	[[nodiscard]] static Type Id() {
		return &SectionMetaImplementation<SectionType>::Meta;
	}
	[[nodiscard]] Type id() const final override {
		return Id();
	}
};

inline constexpr auto kIconRed = 1;
inline constexpr auto kIconGreen = 2;
inline constexpr auto kIconLightOrange = 3;
inline constexpr auto kIconLightBlue = 4;
inline constexpr auto kIconDarkBlue = 5;
inline constexpr auto kIconPurple = 6;
inline constexpr auto kIconDarkOrange = 8;
inline constexpr auto kIconGray = 9;

enum class IconType {
	Rounded,
	Round,
};

struct IconDescriptor {
	const style::icon *icon = nullptr;
	int color = 0; // settingsIconBg{color}, 9 for settingsIconBgArchive.
	IconType type = IconType::Rounded;
	const style::color *background = nullptr;
	std::optional<QBrush> backgroundBrush; // Can be useful for gragdients.

	explicit operator bool() const {
		return (icon != nullptr);
	}
};

class Icon final {
public:
	explicit Icon(IconDescriptor descriptor);

	void paint(QPainter &p, QPoint position) const;
	void paint(QPainter &p, int x, int y) const;

	[[nodiscard]] int width() const;
	[[nodiscard]] int height() const;
	[[nodiscard]] QSize size() const;

private:
	not_null<const style::icon*> _icon;
	std::optional<Ui::RoundRect> _background;
	std::optional<std::pair<int, QBrush>> _backgroundBrush;

};

void AddSkip(not_null<Ui::VerticalLayout*> container);
void AddSkip(not_null<Ui::VerticalLayout*> container, int skip);
void AddDivider(not_null<Ui::VerticalLayout*> container);
void AddDividerText(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text);
void AddButtonIcon(
	not_null<Ui::AbstractButton*> button,
	const style::SettingsButton &st,
	IconDescriptor &&descriptor);
object_ptr<Button> CreateButton(
	not_null<QWidget*> parent,
	rpl::producer<QString> text,
	const style::SettingsButton &st,
	IconDescriptor &&descriptor = {});
not_null<Button*> AddButton(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	const style::SettingsButton &st,
	IconDescriptor &&descriptor = {});
not_null<Button*> AddButtonWithLabel(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	rpl::producer<QString> label,
	const style::SettingsButton &st,
	IconDescriptor &&descriptor = {});
void CreateRightLabel(
	not_null<Button*> button,
	rpl::producer<QString> label,
	const style::SettingsButton &st,
	rpl::producer<QString> buttonText);
not_null<Ui::FlatLabel*> AddSubsectionTitle(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	style::margins addPadding = {},
	const style::FlatLabel *st = nullptr);

struct LottieIcon {
	object_ptr<Ui::RpWidget> widget;
	Fn<void(anim::repeat repeat)> animate;
};
[[nodiscard]] LottieIcon CreateLottieIcon(
	not_null<QWidget*> parent,
	Lottie::IconDescriptor &&descriptor,
	style::margins padding = {});

void FillMenu(
	not_null<Window::SessionController*> controller,
	Type type,
	Fn<void(Type)> showOther,
	Ui::Menu::MenuCallback addAction);

} // namespace Settings
