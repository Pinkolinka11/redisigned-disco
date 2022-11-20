/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/integration.h"

namespace Main {
class Session;
} // namespace Main

namespace HistoryView {
class ElementDelegate;
} // namespace HistoryView

namespace Core {

struct MarkedTextContext {
	enum class HashtagMentionType : uchar {
		Telegram,
		Twitter,
		Instagram,
	};

	Main::Session *session = nullptr;
	HashtagMentionType type = HashtagMentionType::Telegram;
	Fn<void()> customEmojiRepaint;
};

class UiIntegration final : public Ui::Integration {
public:
	void postponeCall(FnMut<void()> &&callable) override;
	void registerLeaveSubscription(not_null<QWidget*> widget) override;
	void unregisterLeaveSubscription(not_null<QWidget*> widget) override;

	QString emojiCacheFolder() override;
	QString openglCheckFilePath() override;
	QString angleBackendFilePath() override;

	void textActionsUpdated() override;
	void activationFromTopPanel() override;

	bool screenIsLocked() override;
	QString timeFormat() override;

	std::shared_ptr<ClickHandler> createLinkHandler(
		const EntityLinkData &data,
		const std::any &context) override;
	bool handleUrlClick(
		const QString &url,
		const QVariant &context) override;
	rpl::producer<> forcePopupMenuHideRequests() override;
	const Ui::Emoji::One *defaultEmojiVariant(
		const Ui::Emoji::One *emoji) override;
	std::unique_ptr<Ui::Text::CustomEmoji> createCustomEmoji(
		const QString &data,
		const std::any &context) override;
	Fn<void()> createSpoilerRepaint(const std::any &context) override;

	QString phraseContextCopyText() override;
	QString phraseContextCopyEmail() override;
	QString phraseContextCopyLink() override;
	QString phraseContextCopySelected() override;
	QString phraseFormattingTitle() override;
	QString phraseFormattingLinkCreate() override;
	QString phraseFormattingLinkEdit() override;
	QString phraseFormattingClear() override;
	QString phraseFormattingBold() override;
	QString phraseFormattingItalic() override;
	QString phraseFormattingUnderline() override;
	QString phraseFormattingStrikeOut() override;
	QString phraseFormattingMonospace() override;
	QString phraseFormattingSpoiler() override;
	QString phraseButtonOk() override;
	QString phraseButtonClose() override;
	QString phraseButtonCancel() override;
	QString phrasePanelCloseWarning() override;
	QString phrasePanelCloseUnsaved() override;
	QString phrasePanelCloseAnyway() override;
#if 0 // disabled for now
	QString phraseBotSharePhone() override;
	QString phraseBotSharePhoneTitle() override;
	QString phraseBotSharePhoneConfirm() override;
#endif

};

[[nodiscard]] bool OpenGLLastCheckFailed();

} // namespace Core
