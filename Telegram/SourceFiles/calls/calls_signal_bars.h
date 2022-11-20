/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace style {
struct CallSignalBars;
} // namespace style

namespace Calls {

class Call;

class SignalBars final : public Ui::RpWidget {
public:
	SignalBars(
		QWidget *parent,
		not_null<Call*> call,
		const style::CallSignalBars &st);

private:
	void paintEvent(QPaintEvent *e) override;

	void changed(int count);

	const style::CallSignalBars &_st;
	int _count = 0;

};

} // namespace Calls
