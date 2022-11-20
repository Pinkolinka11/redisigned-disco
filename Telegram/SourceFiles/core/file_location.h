/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QDateTime>

class QFileInfo;

namespace Platform {
class FileBookmark;
} // namespace Platform

namespace Core {

class ReadAccessEnabler {
public:
	ReadAccessEnabler(const Platform::FileBookmark *bookmark);
	ReadAccessEnabler(
		const std::shared_ptr<Platform::FileBookmark> &bookmark);
	bool failed() const {
		return _failed;
	}
	~ReadAccessEnabler();

private:
	const Platform::FileBookmark *_bookmark = nullptr;
	bool _failed;

};

class FileLocation {
public:
	FileLocation() = default;
	explicit FileLocation(const QString &name);
	explicit FileLocation(const QFileInfo &info);

	static FileLocation InMediaCacheLocation();

	[[nodiscard]] bool check() const;
	[[nodiscard]] const QString &name() const;
	void setBookmark(const QByteArray &bookmark);
	QByteArray bookmark() const;
	[[nodiscard]] bool isEmpty() const {
		return name().isEmpty();
	}
	[[nodiscard]] bool inMediaCache() const;

	bool accessEnable() const;
	void accessDisable() const;

	QString fname;
	QDateTime modified;
	qint64 size = 0;

private:
	void resolveFromInfo(const QFileInfo &info);

	std::shared_ptr<Platform::FileBookmark> _bookmark;

};

inline bool operator==(const FileLocation &a, const FileLocation &b) {
	return (a.name() == b.name())
		&& (a.modified == b.modified)
		&& (a.size == b.size);
}

inline bool operator!=(const FileLocation &a, const FileLocation &b) {
	return !(a == b);
}

} // namespace Core
