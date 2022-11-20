/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "boxes/peer_list_box.h"
#include "base/flat_set.h"
#include "base/weak_ptr.h"
#include "base/timer.h"

// Not used for now.
//
//class MembersAddButton : public Ui::RippleButton {
//public:
//	MembersAddButton(QWidget *parent, const style::TwoIconButton &st);
//
//protected:
//	void paintEvent(QPaintEvent *e) override;
//
//	QImage prepareRippleMask() const override;
//	QPoint prepareRippleStartPosition() const override;
//
//private:
//	const style::TwoIconButton &_st;
//
//};

class History;

namespace Window {
class SessionController;
} // namespace Window

[[nodiscard]] object_ptr<Ui::BoxContent> PrepareContactsBox(
	not_null<Window::SessionController*> sessionController);

class PeerListRowWithLink : public PeerListRow {
public:
	using PeerListRow::PeerListRow;

	void setActionLink(const QString &action);

	void lazyInitialize(const style::PeerListItem &st) override;

protected:
	QSize rightActionSize() const override;
	QMargins rightActionMargins() const override;
	void rightActionPaint(
		Painter &p,
		int x,
		int y,
		int outerWidth,
		bool selected,
		bool actionSelected) override;

private:
	void refreshActionLink();

	QString _action;
	int _actionWidth = 0;

};

class PeerListGlobalSearchController : public PeerListSearchController {
public:
	explicit PeerListGlobalSearchController(not_null<Main::Session*> session);

	void searchQuery(const QString &query) override;
	bool isLoading() override;
	bool loadMoreRows() override {
		return false;
	}

private:
	bool searchInCache();
	void searchOnServer();
	void searchDone(const MTPcontacts_Found &result, mtpRequestId requestId);

	const not_null<Main::Session*> _session;
	MTP::Sender _api;
	base::Timer _timer;
	QString _query;
	mtpRequestId _requestId = 0;
	std::map<QString, MTPcontacts_Found> _cache;
	std::map<mtpRequestId, QString> _queries;

};

class ChatsListBoxController : public PeerListController {
public:
	class Row : public PeerListRow {
	public:
		Row(not_null<History*> history);

		not_null<History*> history() const {
			return _history;
		}

	private:
		not_null<History*> _history;

	};

	ChatsListBoxController(not_null<Main::Session*> session);
	ChatsListBoxController(
		std::unique_ptr<PeerListSearchController> searchController);

	void prepare() override final;
	std::unique_ptr<PeerListRow> createSearchRow(not_null<PeerData*> peer) override final;

protected:
	virtual std::unique_ptr<Row> createRow(not_null<History*> history) = 0;
	virtual void prepareViewHook() = 0;
	virtual void updateRowHook(not_null<Row*> row) {
	}
	virtual QString emptyBoxText() const;

private:
	void rebuildRows();
	void checkForEmptyRows();
	bool appendRow(not_null<History*> history);

};

class ContactsBoxController : public PeerListController {
public:
	explicit ContactsBoxController(not_null<Main::Session*> session);
	ContactsBoxController(
		not_null<Main::Session*> session,
		std::unique_ptr<PeerListSearchController> searchController);

	[[nodiscard]] Main::Session &session() const override;
	void prepare() override final;
	[[nodiscard]] std::unique_ptr<PeerListRow> createSearchRow(
		not_null<PeerData*> peer) override final;
	void rowClicked(not_null<PeerListRow*> row) override;

	enum class SortMode {
		Alphabet,
		Online,
	};
	void setSortMode(SortMode mode);

protected:
	virtual std::unique_ptr<PeerListRow> createRow(not_null<UserData*> user);
	virtual void prepareViewHook() {
	}
	virtual void updateRowHook(not_null<PeerListRow*> row) {
	}

private:
	void sort();
	void sortByName();
	void sortByOnline();
	void rebuildRows();
	void checkForEmptyRows();
	bool appendRow(not_null<UserData*> user);

	const not_null<Main::Session*> _session;
	SortMode _sortMode = SortMode::Alphabet;
	base::Timer _sortByOnlineTimer;
	rpl::lifetime _sortByOnlineLifetime;

};

class ChooseRecipientBoxController
	: public ChatsListBoxController
	, public base::has_weak_ptr {
public:
	ChooseRecipientBoxController(
		not_null<Main::Session*> session,
		FnMut<void(not_null<PeerData*>)> callback,
		Fn<bool(not_null<PeerData*>)> filter = nullptr);

	Main::Session &session() const override;
	void rowClicked(not_null<PeerListRow*> row) override;

	bool respectSavedMessagesChat() const override {
		return true;
	}

protected:
	void prepareViewHook() override;
	std::unique_ptr<Row> createRow(not_null<History*> history) override;

private:
	const not_null<Main::Session*> _session;
	FnMut<void(not_null<PeerData*>)> _callback;
	Fn<bool(not_null<PeerData*>)> _filter;

};
