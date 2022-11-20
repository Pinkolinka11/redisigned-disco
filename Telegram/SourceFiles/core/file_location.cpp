/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/file_location.h"

#include "platform/platform_file_bookmark.h"
#include "logs.h"

#include <QtCore/QFileInfo>

namespace Core {
namespace {

const auto kInMediaCacheLocation = u"*media_cache*"_q;
constexpr auto kMaxFileSize = 4000 * int64(1024 * 1024);

} // namespace

ReadAccessEnabler::ReadAccessEnabler(const Platform::FileBookmark *bookmark)
: _bookmark(bookmark)
, _failed(_bookmark ? !_bookmark->enable() : false) {
}

ReadAccessEnabler::ReadAccessEnabler(
	const std::shared_ptr<Platform::FileBookmark> &bookmark)
: _bookmark(bookmark.get())
, _failed(_bookmark ? !_bookmark->enable() : false) {
}

ReadAccessEnabler::~ReadAccessEnabler() {
	if (_bookmark && !_failed) _bookmark->disable();
}

FileLocation::FileLocation(const QString &name) : fname(name) {
	if (fname.isEmpty() || fname == kInMediaCacheLocation) {
		size = 0;
	} else {
		setBookmark(Platform::PathBookmark(name));
		resolveFromInfo(QFileInfo(name));
	}
}

FileLocation::FileLocation(const QFileInfo &info) : fname(info.filePath()) {
	if (fname.isEmpty()) {
		size = 0;
	} else {
		setBookmark(Platform::PathBookmark(fname));
		resolveFromInfo(info);
	}
}

void FileLocation::resolveFromInfo(const QFileInfo &info) {
	if (info.exists()) {
		const auto s = info.size();
		if (s > kMaxFileSize) {
			fname = QString();
			_bookmark = nullptr;
			size = 0;
		} else {
			modified = info.lastModified();
			size = s;
		}
	} else {
		fname = QString();
		_bookmark = nullptr;
		size = 0;
	}
}

FileLocation FileLocation::InMediaCacheLocation() {
	return FileLocation(kInMediaCacheLocation);
}

bool FileLocation::check() const {
	if (fname.isEmpty() || fname == kInMediaCacheLocation) {
		return false;
	}

	ReadAccessEnabler enabler(_bookmark);
	if (enabler.failed()) {
		const_cast<FileLocation*>(this)->_bookmark = nullptr;
	}

	QFileInfo f(name());
	if (!f.isReadable()) return false;

	quint64 s = f.size();
	if (s > kMaxFileSize) {
		DEBUG_LOG(("File location check: Wrong size %1").arg(s));
		return false;
	}

	if (s != size) {
		DEBUG_LOG(("File location check: Wrong size %1 when should be %2").arg(s).arg(size));
		return false;
	}
	auto realModified = f.lastModified();
	if (realModified != modified) {
		DEBUG_LOG(("File location check: Wrong last modified time %1 when should be %2").arg(realModified.toMSecsSinceEpoch()).arg(modified.toMSecsSinceEpoch()));
		return false;
	}
	return true;
}

const QString &FileLocation::name() const {
	return _bookmark ? _bookmark->name(fname) : fname;
}

QByteArray FileLocation::bookmark() const {
	return _bookmark ? _bookmark->bookmark() : QByteArray();
}

bool FileLocation::inMediaCache() const {
	return (fname == kInMediaCacheLocation);
}

void FileLocation::setBookmark(const QByteArray &bm) {
	_bookmark.reset(bm.isEmpty() ? nullptr : new Platform::FileBookmark(bm));
}

bool FileLocation::accessEnable() const {
	return isEmpty() ? false : (_bookmark ? _bookmark->enable() : true);
}

void FileLocation::accessDisable() const {
	return _bookmark ? _bookmark->disable() : (void)0;
}

} // namespace Core
