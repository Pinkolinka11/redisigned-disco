/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_common.h"
#include "ui/effects/animations.h"
#include "base/timer.h"

namespace style {
struct Checkbox;
struct Radio;
} // namespace style

namespace Calls {
class Call;
} // namespace Calls

namespace Ui {
class LevelMeter;
class GenericBox;
class Show;
} // namespace Ui

namespace Webrtc {
class AudioInputTester;
class VideoTrack;
} // namespace Webrtc

namespace Settings {

class Calls : public Section<Calls> {
public:
	Calls(QWidget *parent, not_null<Window::SessionController*> controller);
	~Calls();

	[[nodiscard]] rpl::producer<QString> title() override;

	void sectionSaveChanges(FnMut<void()> done) override;

	static Webrtc::VideoTrack *AddCameraSubsection(
		std::shared_ptr<Ui::Show> show,
		not_null<Ui::VerticalLayout*> content,
		bool saveToSettings);

private:
	void setupContent();
	void requestPermissionAndStartTestingMicrophone();
	void startTestingMicrophone();

	const not_null<Window::SessionController*> _controller;
	rpl::event_stream<QString> _cameraNameStream;
	rpl::event_stream<QString> _outputNameStream;
	rpl::event_stream<QString> _inputNameStream;
	std::unique_ptr<Webrtc::AudioInputTester> _micTester;
	Ui::LevelMeter *_micTestLevel = nullptr;
	float _micLevel = 0.;
	Ui::Animations::Simple _micLevelAnimation;
	base::Timer _levelUpdateTimer;

};

inline constexpr auto kMicTestUpdateInterval = crl::time(100);
inline constexpr auto kMicTestAnimationDuration = crl::time(200);

[[nodiscard]] QString CurrentAudioOutputName();
[[nodiscard]] QString CurrentAudioInputName();
[[nodiscard]] object_ptr<Ui::GenericBox> ChooseAudioOutputBox(
	Fn<void(QString id, QString name)> chosen,
	const style::Checkbox *st = nullptr,
	const style::Radio *radioSt = nullptr);
[[nodiscard]] object_ptr<Ui::GenericBox> ChooseAudioInputBox(
	Fn<void(QString id, QString name)> chosen,
	const style::Checkbox *st = nullptr,
	const style::Radio *radioSt = nullptr);
//[[nodiscard]] object_ptr<Ui::GenericBox> ChooseAudioBackendBox(
//	const style::Checkbox *st = nullptr,
//	const style::Radio *radioSt = nullptr);

} // namespace Settings

