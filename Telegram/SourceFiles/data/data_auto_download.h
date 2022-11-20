/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <array>

namespace Data {
namespace AutoDownload {

constexpr auto kMaxBytesLimit = 8000 * int64(512 * 1024);

enum class Source {
	User    = 0x00,
	Group   = 0x01,
	Channel = 0x02,
};

constexpr auto kSourcesCount = 3;

enum class Type {
	Photo                = 0x00,
	AutoPlayVideo        = 0x01,
	VoiceMessage         = 0x02,
	AutoPlayVideoMessage = 0x03,
	Music                = 0x04,
	AutoPlayGIF          = 0x05,
	File                 = 0x06,
};

inline constexpr auto kAutoPlayTypes = {
	Type::AutoPlayVideo,
	Type::AutoPlayVideoMessage,
	Type::AutoPlayGIF,
};

inline constexpr auto kStreamedTypes = {
	Type::VoiceMessage,
	Type::Music,
};

constexpr auto kTypesCount = 7;

class Single {
public:
	void setBytesLimit(int64 bytesLimit);

	bool hasValue() const;
	bool shouldDownload(int64 fileSize) const;
	int64 bytesLimit() const;

	qint32 serialize() const;
	bool setFromSerialized(qint32 serialized);

private:
	int _limit = -1; // FileSize: Right now any file size fits 32 bit.

};

class Set {
public:
	void setBytesLimit(Type type, int64 bytesLimit);

	bool hasValue(Type type) const;
	bool shouldDownload(Type type, int64 fileSize) const;
	int64 bytesLimit(Type type) const;

	qint32 serialize(Type type) const;
	bool setFromSerialized(Type type, qint32 serialized);

private:
	const Single &single(Type type) const;
	Single &single(Type type);

	std::array<Single, kTypesCount> _data;

};

class Full {
public:
	void setBytesLimit(Source source, Type type, int64 bytesLimit);

	[[nodiscard]] bool shouldDownload(
		Source source,
		Type type,
		int64 fileSize) const;
	[[nodiscard]] int64 bytesLimit(Source source, Type type) const;

	[[nodiscard]] QByteArray serialize() const;
	bool setFromSerialized(const QByteArray &serialized);

	[[nodiscard]] static Full FullDisabled();

private:
	[[nodiscard]] const Set &set(Source source) const;
	[[nodiscard]] Set &set(Source source);
	[[nodiscard]] const Set &setOrDefault(Source source, Type type) const;

	std::array<Set, kSourcesCount> _data;

};

[[nodiscard]] bool Should(
	const Full &data,
	not_null<PeerData*> peer,
	not_null<DocumentData*> document);
[[nodiscard]] bool Should(
	const Full &data,
	not_null<DocumentData*> document);
[[nodiscard]] bool Should(
	const Full &data,
	not_null<PeerData*> peer,
	not_null<PhotoData*> photo);

[[nodiscard]] bool ShouldAutoPlay(
	const Full &data,
	not_null<PeerData*> peer,
	not_null<DocumentData*> document);
[[nodiscard]] bool ShouldAutoPlay(
	const Full &data,
	not_null<PeerData*> peer,
	not_null<PhotoData*> photo);

[[nodiscard]] Full WithDisabledAutoPlay(const Full &data);

} // namespace AutoDownload
} // namespace Data
