/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/stickers/data_custom_emoji.h"

#include "chat_helpers/stickers_emoji_pack.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_peer.h"
#include "data/data_message_reactions.h"
#include "data/stickers/data_stickers.h"
#include "lottie/lottie_common.h"
#include "lottie/lottie_frame_generator.h"
#include "ffmpeg/ffmpeg_frame_generator.h"
#include "chat_helpers/stickers_lottie.h"
#include "ui/widgets/input_fields.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/ui_utility.h"
#include "apiwrap.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"

namespace Data {
namespace {

constexpr auto kMaxPerRequest = 100;

using SizeTag = CustomEmojiManager::SizeTag;

[[nodiscard]] ChatHelpers::StickerLottieSize LottieSizeFromTag(SizeTag tag) {
	// NB! onlyCustomEmoji dimensions caching uses last ::EmojiInteraction-s.
	using LottieSize = ChatHelpers::StickerLottieSize;
	switch (tag) {
	case SizeTag::Normal: return LottieSize::EmojiInteraction;
	case SizeTag::Large: return LottieSize::EmojiInteractionReserved1;
	case SizeTag::Isolated: return LottieSize::EmojiInteractionReserved2;
	}
	Unexpected("SizeTag value in CustomEmojiManager-LottieSizeFromTag.");
}

[[nodiscard]] int EmojiSizeFromTag(SizeTag tag) {
	switch (tag) {
	case SizeTag::Normal: return Ui::Emoji::GetSizeNormal();
	case SizeTag::Large: return Ui::Emoji::GetSizeLarge();
	case SizeTag::Isolated:
		return (st::largeEmojiSize + 2 * st::largeEmojiOutline)
			* style::DevicePixelRatio();
	}
	Unexpected("SizeTag value in CustomEmojiManager-SizeFromTag.");
}

[[nodiscard]] int FrameSizeFromTag(SizeTag tag, int sizeOverride) {
	return sizeOverride
		? (sizeOverride * style::DevicePixelRatio())
		: FrameSizeFromTag(tag);
}

} // namespace

class CustomEmojiLoader final
	: public Ui::CustomEmoji::Loader
	, public base::has_weak_ptr {
public:
	CustomEmojiLoader(
		not_null<Session*> owner,
		const CustomEmojiId id,
		SizeTag tag,
		int sizeOverride);
	CustomEmojiLoader(
		not_null<DocumentData*> document,
		SizeTag tag,
		int sizeOverride);

	[[nodiscard]] DocumentData *document() const;
	void resolved(not_null<DocumentData*> document);

	QString entityData() override;

	void load(Fn<void(LoadResult)> loaded) override;
	bool loading() override;
	void cancel() override;
	Ui::CustomEmoji::Preview preview() override;

private:
	struct Resolve {
		Fn<void(LoadResult)> requested;
		QString entityData;
	};
	struct Process {
		std::shared_ptr<DocumentMedia> media;
		Fn<void(LoadResult)> loaded;
		base::has_weak_ptr guard;
		rpl::lifetime lifetime;
	};
	struct Requested {
		not_null<DocumentData*> document;
		std::unique_ptr<Process> process;
	};
	struct Lookup : Requested {
	};
	struct Load : Requested {
	};

	void check();
	[[nodiscard]] Storage::Cache::Key cacheKey(
		not_null<DocumentData*> document) const;
	void startCacheLookup(
		not_null<Lookup*> lookup,
		Fn<void(LoadResult)> loaded);
	void lookupDone(
		not_null<Lookup*> lookup,
		std::optional<Ui::CustomEmoji::Cache> result);
	void loadNoCache(
		not_null<DocumentData*> document,
		Fn<void(LoadResult)> loaded);

	[[nodiscard]] static std::variant<Resolve, Lookup, Load> InitialState(
		not_null<Session*> owner,
		const CustomEmojiId &id);

	std::variant<Resolve, Lookup, Load> _state;
	ushort _sizeOverride = 0;
	SizeTag _tag = SizeTag::Normal;

};

CustomEmojiLoader::CustomEmojiLoader(
	not_null<Session*> owner,
	const CustomEmojiId id,
	SizeTag tag,
	int sizeOverride)
: _state(InitialState(owner, id))
, _sizeOverride(sizeOverride)
, _tag(tag) {
	Expects(sizeOverride >= 0
		&& sizeOverride <= std::numeric_limits<ushort>::max());
}

CustomEmojiLoader::CustomEmojiLoader(
	not_null<DocumentData*> document,
	SizeTag tag,
	int sizeOverride)
: _state(Lookup{ document })
, _sizeOverride(sizeOverride)
, _tag(tag) {
	Expects(sizeOverride >= 0
		&& sizeOverride <= std::numeric_limits<ushort>::max());
}

DocumentData *CustomEmojiLoader::document() const {
	return v::match(_state, [](const Resolve &) {
		return (DocumentData*)nullptr;
	}, [](const auto &data) {
		return data.document.get();
	});
}

void CustomEmojiLoader::resolved(not_null<DocumentData*> document) {
	Expects(v::is<Resolve>(_state));

	auto requested = std::move(v::get<Resolve>(_state).requested);
	_state = Lookup{ document };
	if (requested) {
		load(std::move(requested));
	}
}

void CustomEmojiLoader::load(Fn<void(LoadResult)> loaded) {
	if (const auto resolve = std::get_if<Resolve>(&_state)) {
		resolve->requested = std::move(loaded);
	} else if (const auto lookup = std::get_if<Lookup>(&_state)) {
		if (!lookup->process) {
			startCacheLookup(lookup, std::move(loaded));
		} else {
			lookup->process->loaded = std::move(loaded);
		}
	} else if (const auto load = std::get_if<Load>(&_state)) {
		if (!load->process) {
			load->process = std::make_unique<Process>(Process{
				.media = load->document->createMediaView(),
				.loaded = std::move(loaded),
			});
			load->process->media->owner()->resetCancelled();
			load->process->media->checkStickerLarge();
			if (load->process->media->loaded()) {
				check();
			} else {
				load->document->session().downloaderTaskFinished(
				) | rpl::start_with_next([=] {
					check();
				}, load->process->lifetime);
			}
		} else {
			load->process->loaded = std::move(loaded);
		}
	}
}

QString CustomEmojiLoader::entityData() {
	if (const auto resolve = std::get_if<Resolve>(&_state)) {
		return resolve->entityData;
	} else if (const auto lookup = std::get_if<Lookup>(&_state)) {
		return SerializeCustomEmojiId(lookup->document);
	} else if (const auto load = std::get_if<Load>(&_state)) {
		return SerializeCustomEmojiId(load->document);
	}
	Unexpected("State in CustomEmojiLoader::entityData.");
}

bool CustomEmojiLoader::loading() {
	if (const auto resolve = std::get_if<Resolve>(&_state)) {
		return (resolve->requested != nullptr);
	} else if (const auto lookup = std::get_if<Lookup>(&_state)) {
		return (lookup->process != nullptr);
	} else if (const auto load = std::get_if<Load>(&_state)) {
		return (load->process != nullptr);
	}
	return false;
}

Storage::Cache::Key CustomEmojiLoader::cacheKey(
		not_null<DocumentData*> document) const {
	const auto baseKey = document->bigFileBaseCacheKey();
	if (!baseKey) {
		return {};
	}
	return Storage::Cache::Key{
		baseKey.high,
		baseKey.low + ChatHelpers::LottieCacheKeyShift(
			0x0F,
			LottieSizeFromTag(_tag)),
	};
}

void CustomEmojiLoader::startCacheLookup(
		not_null<Lookup*> lookup,
		Fn<void(LoadResult)> loaded) {
	const auto document = lookup->document;
	const auto key = cacheKey(document);
	if (!key) {
		loadNoCache(document, std::move(loaded));
		return;
	}
	lookup->process = std::make_unique<Process>(Process{
		.loaded = std::move(loaded),
	});
	const auto size = FrameSizeFromTag(_tag, _sizeOverride);
	const auto weak = base::make_weak(&lookup->process->guard);
	document->owner().cacheBigFile().get(key, [=](QByteArray value) {
		auto cache = Ui::CustomEmoji::Cache::FromSerialized(value, size);
		crl::on_main(weak, [=, result = std::move(cache)]() mutable {
			lookupDone(lookup, std::move(result));
		});
	});
}

void CustomEmojiLoader::lookupDone(
		not_null<Lookup*> lookup,
		std::optional<Ui::CustomEmoji::Cache> result) {
	const auto document = lookup->document;
	if (!result) {
		loadNoCache(document, std::move(lookup->process->loaded));
		return;
	}
	const auto tag = _tag;
	const auto sizeOverride = int(_sizeOverride);
	auto loader = [=] {
		return std::make_unique<CustomEmojiLoader>(
			document,
			tag,
			sizeOverride);
	};
	auto done = std::move(lookup->process->loaded);
	done(Ui::CustomEmoji::Cached(
		SerializeCustomEmojiId(document),
		std::move(loader),
		std::move(*result)));
}

void CustomEmojiLoader::loadNoCache(
		not_null<DocumentData*> document,
		Fn<void(LoadResult)> loaded) {
	_state = Load{ document };
	load(std::move(loaded));
}

void CustomEmojiLoader::check() {
	using namespace Ui::CustomEmoji;

	const auto load = std::get_if<Load>(&_state);
	Assert(load != nullptr);
	Assert(load->process != nullptr);

	const auto media = load->process->media.get();
	const auto document = media->owner();
	const auto data = media->bytes();
	const auto filepath = document->filepath();
	if (data.isEmpty() && filepath.isEmpty()) {
		return;
	}
	load->process->lifetime.destroy();

	const auto tag = _tag;
	const auto sizeOverride = int(_sizeOverride);
	const auto size = FrameSizeFromTag(_tag, _sizeOverride);
	auto bytes = Lottie::ReadContent(data, filepath);
	auto loader = [=] {
		return std::make_unique<CustomEmojiLoader>(
			document,
			tag,
			sizeOverride);
	};
	auto put = [=, key = cacheKey(document)](QByteArray value) {
		document->owner().cacheBigFile().put(key, std::move(value));
	};
	const auto type = document->sticker()->type;
	auto generator = [=, bytes = Lottie::ReadContent(data, filepath)]()
	-> std::unique_ptr<Ui::FrameGenerator> {
		switch (type) {
		case StickerType::Tgs:
			return std::make_unique<Lottie::FrameGenerator>(bytes);
		case StickerType::Webm:
			return std::make_unique<FFmpeg::FrameGenerator>(bytes);
		case StickerType::Webp:
			return std::make_unique<Ui::ImageFrameGenerator>(bytes);
		}
		Unexpected("Type in custom emoji sticker frame generator.");
	};
	auto renderer = std::make_unique<Renderer>(RendererDescriptor{
		.generator = std::move(generator),
		.put = std::move(put),
		.loader = std::move(loader),
		.size = size,
	});
	base::take(load->process)->loaded(Caching{
		std::move(renderer),
		SerializeCustomEmojiId(document),
	});
}

auto CustomEmojiLoader::InitialState(
	not_null<Session*> owner,
	const CustomEmojiId &id)
-> std::variant<Resolve, Lookup, Load> {
	const auto document = owner->document(id.id);
	if (document->sticker()) {
		return Lookup{ document };
	}
	return Resolve{ .entityData = SerializeCustomEmojiId(id) };
}

void CustomEmojiLoader::cancel() {
	if (const auto lookup = std::get_if<Lookup>(&_state)) {
		base::take(lookup->process);
	} else if (const auto load = std::get_if<Load>(&_state)) {
		if (base::take(load->process)) {
			load->document->cancel();
		}
	}
}

Ui::CustomEmoji::Preview CustomEmojiLoader::preview() {
	using Preview = Ui::CustomEmoji::Preview;
	const auto make = [&](not_null<DocumentData*> document) -> Preview {
		const auto dimensions = document->dimensions;
		if (!document->inlineThumbnailIsPath()
			|| !dimensions.width()) {
			return {};
		}
		const auto scale = (FrameSizeFromTag(_tag, _sizeOverride) * 1.)
			/ (style::DevicePixelRatio() * dimensions.width());
		return { document->createMediaView()->thumbnailPath(), scale };
	};
	if (const auto lookup = std::get_if<Lookup>(&_state)) {
		return make(lookup->document);
	} else if (const auto load = std::get_if<Load>(&_state)) {
		return make(load->document);
	}
	return {};
}

CustomEmojiManager::CustomEmojiManager(not_null<Session*> owner)
: _owner(owner)
, _repaintTimer([=] { invokeRepaints(); }) {
	const auto appConfig = &owner->session().account().appConfig();
	appConfig->value(
	) | rpl::take_while([=] {
		return !_coloredSetId;
	}) | rpl::start_with_next([=] {
		const auto setId = appConfig->get<QString>(
			"default_emoji_statuses_stickerset_id",
			QString()).toULongLong();
		if (setId) {
			_coloredSetId = setId;
			auto pending = base::take(_coloredSetPending);
			for (const auto &instance : pending[setId]) {
				instance->setColored();
			}
		}
	}, _lifetime);
}

CustomEmojiManager::~CustomEmojiManager() = default;

template <typename LoaderFactory>
std::unique_ptr<Ui::Text::CustomEmoji> CustomEmojiManager::create(
		DocumentId documentId,
		Fn<void()> update,
		SizeTag tag,
		int sizeOverride,
		LoaderFactory factory) {
	auto &instances = _instances[SizeIndex(tag)];
	auto i = instances.find(documentId);
	if (i == end(instances)) {
		using Loading = Ui::CustomEmoji::Loading;
		const auto repaint = [=](
				not_null<Ui::CustomEmoji::Instance*> instance,
				Ui::CustomEmoji::RepaintRequest request) {
			repaintLater(instance, request);
		};
		auto [loader, setId] = factory();
		i = instances.emplace(
			documentId,
			std::make_unique<Ui::CustomEmoji::Instance>(Loading{
				std::move(loader),
				prepareNonExactPreview(documentId, tag, sizeOverride)
			}, std::move(repaint))).first;
		if (_coloredSetId) {
			if (_coloredSetId == setId) {
				i->second->setColored();
			}
		} else if (setId) {
			_coloredSetPending[setId].emplace(i->second.get());
		}
	} else if (!i->second->hasImagePreview()) {
		auto preview = prepareNonExactPreview(documentId, tag, sizeOverride);
		if (preview.isImage()) {
			i->second->updatePreview(std::move(preview));
		}
	}
	return std::make_unique<Ui::CustomEmoji::Object>(
		i->second.get(),
		std::move(update));
}

Ui::CustomEmoji::Preview CustomEmojiManager::prepareNonExactPreview(
		DocumentId documentId,
		SizeTag tag,
		int sizeOverride) const {
	for (auto i = _instances.size(); i != 0;) {
		if (SizeIndex(tag) == --i) {
			continue;
		}
		const auto &other = _instances[i];
		const auto j = other.find(documentId);
		if (j == end(other)) {
			continue;
		} else if (const auto nonExact = j->second->imagePreview()) {
			const auto size = FrameSizeFromTag(tag, sizeOverride);
			return {
				nonExact.image().scaled(
					size,
					size,
					Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation),
				false,
			};
		}
	}
	return {};
}

std::unique_ptr<Ui::Text::CustomEmoji> CustomEmojiManager::create(
		QStringView data,
		Fn<void()> update,
		SizeTag tag,
		int sizeOverride) {
	const auto parsed = ParseCustomEmojiData(data);
	return parsed.id
		? create(parsed.id, std::move(update), tag, sizeOverride)
		: nullptr;
}

std::unique_ptr<Ui::Text::CustomEmoji> CustomEmojiManager::create(
		DocumentId documentId,
		Fn<void()> update,
		SizeTag tag,
		int sizeOverride) {
	return create(documentId, std::move(update), tag, sizeOverride, [&] {
		return createLoaderWithSetId(documentId, tag, sizeOverride);
	});
}

std::unique_ptr<Ui::Text::CustomEmoji> CustomEmojiManager::create(
		not_null<DocumentData*> document,
		Fn<void()> update,
		SizeTag tag,
		int sizeOverride) {
	return create(document->id, std::move(update), tag, sizeOverride, [&] {
		return createLoaderWithSetId(document, tag, sizeOverride);
	});
}

void CustomEmojiManager::resolve(
		QStringView data,
		not_null<Listener*> listener) {
	resolve(ParseCustomEmojiData(data).id, listener);
}

void CustomEmojiManager::resolve(
		DocumentId documentId,
		not_null<Listener*> listener) {
	if (_owner->document(documentId)->sticker()) {
		return;
	}
	_resolvers[documentId].emplace(listener);
	_listeners[listener].emplace(documentId);
	_pendingForRequest.emplace(documentId);
	if (!_requestId && _pendingForRequest.size() == 1) {
		crl::on_main(this, [=] { request(); });
	}
}

void CustomEmojiManager::unregisterListener(not_null<Listener*> listener) {
	if (const auto list = _listeners.take(listener)) {
		for (const auto id : *list) {
			const auto i = _resolvers.find(id);
			if (i != end(_resolvers)
				&& i->second.remove(listener)
				&& i->second.empty()) {
				_resolvers.erase(i);
			}
		}
	}
}

std::unique_ptr<Ui::CustomEmoji::Loader> CustomEmojiManager::createLoader(
		not_null<DocumentData*> document,
		SizeTag tag,
		int sizeOverride) {
	return createLoaderWithSetId(document, tag, sizeOverride).loader;
}

std::unique_ptr<Ui::CustomEmoji::Loader> CustomEmojiManager::createLoader(
		DocumentId documentId,
		SizeTag tag,
		int sizeOverride) {
	return createLoaderWithSetId(documentId, tag, sizeOverride).loader;
}

auto CustomEmojiManager::createLoaderWithSetId(
	not_null<DocumentData*> document,
	SizeTag tag,
	int sizeOverride
) -> LoaderWithSetId {
	if (const auto sticker = document->sticker()) {
		return {
			std::make_unique<CustomEmojiLoader>(document, tag, sizeOverride),
			sticker->set.id,
		};
	}
	return createLoaderWithSetId(document->id, tag, sizeOverride);
}

auto CustomEmojiManager::createLoaderWithSetId(
	DocumentId documentId,
	SizeTag tag,
	int sizeOverride
) -> LoaderWithSetId {
	auto result = std::make_unique<CustomEmojiLoader>(
		_owner,
		CustomEmojiId{ .id = documentId },
		tag,
		sizeOverride);
	if (const auto document = result->document()) {
		if (const auto sticker = document->sticker()) {
			return { std::move(result), sticker->set.id };
		}
	} else {
		const auto i = SizeIndex(tag);
		_loaders[i][documentId].push_back(base::make_weak(result.get()));
		_pendingForRequest.emplace(documentId);
		if (!_requestId && _pendingForRequest.size() == 1) {
			crl::on_main(this, [=] { request(); });
		}
	}
	return { std::move(result), uint64() };
}

QString CustomEmojiManager::lookupSetName(uint64 setId) {
	const auto &sets = _owner->stickers().sets();
	const auto i = sets.find(setId);
	return (i != end(sets)) ? i->second->title : QString();
}

void CustomEmojiManager::request() {
	auto ids = QVector<MTPlong>();
	ids.reserve(std::min(kMaxPerRequest, int(_pendingForRequest.size())));
	while (!_pendingForRequest.empty() && ids.size() < kMaxPerRequest) {
		const auto i = _pendingForRequest.end() - 1;
		ids.push_back(MTP_long(*i));
		_pendingForRequest.erase(i);
	}
	if (ids.isEmpty()) {
		return;
	}
	const auto api = &_owner->session().api();
	_requestId = api->request(MTPmessages_GetCustomEmojiDocuments(
		MTP_vector<MTPlong>(ids)
	)).done([=](const MTPVector<MTPDocument> &result) {
		for (const auto &entry : result.v) {
			const auto document = _owner->processDocument(entry);
			fillColoredFlags(document);
			processLoaders(document);
			processListeners(document);
			requestSetFor(document);
		}
		requestFinished();
	}).fail([=] {
		LOG(("API Error: Failed to get documents for emoji."));
		requestFinished();
	}).send();
}

void CustomEmojiManager::fillColoredFlags(not_null<DocumentData*> document) {
	const auto id = document->id;
	const auto sticker = document->sticker();
	const auto setId = sticker ? sticker->set.id : uint64();
	if (!setId || (_coloredSetId && setId != _coloredSetId)) {
		return;
	}
	for (auto &instances : _instances) {
		const auto i = instances.find(id);
		if (i != end(instances)) {
			if (setId == _coloredSetId) {
				i->second->setColored();
			} else {
				_coloredSetPending[setId].emplace(i->second.get());
			}
		}
	}
}

void CustomEmojiManager::processLoaders(not_null<DocumentData*> document) {
	const auto id = document->id;
	for (auto &loaders : _loaders) {
		if (const auto list = loaders.take(id)) {
			for (const auto &weak : *list) {
				if (const auto strong = weak.get()) {
					strong->resolved(document);
				}
			}
		}
	}
}

void CustomEmojiManager::processListeners(not_null<DocumentData*> document) {
	const auto id = document->id;
	if (const auto listeners = _resolvers.take(id)) {
		for (const auto &listener : *listeners) {
			const auto i = _listeners.find(listener);
			if (i != end(_listeners) && i->second.remove(id)) {
				if (i->second.empty()) {
					_listeners.erase(i);
				}
				listener->customEmojiResolveDone(document);
			}
		}
	}
}

void CustomEmojiManager::requestSetFor(not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	if (!sticker || !sticker->set.id) {
		return;
	}
	const auto &sets = document->owner().stickers().sets();
	const auto i = sets.find(sticker->set.id);
	if (i != end(sets)) {
		return;
	}
	const auto session = &document->session();
	session->api().scheduleStickerSetRequest(
		sticker->set.id,
		sticker->set.accessHash);
	if (_requestSetsScheduled) {
		return;
	}
	_requestSetsScheduled = true;
	crl::on_main(this, [=] {
		_requestSetsScheduled = false;
		session->api().requestStickerSets();
	});
}

int CustomEmojiManager::SizeIndex(SizeTag tag) {
	const auto result = static_cast<int>(tag);

	Ensures(result >= 0 && result < kSizeCount);
	return result;
}

void CustomEmojiManager::requestFinished() {
	_requestId = 0;
	if (!_pendingForRequest.empty()) {
		request();
	}
}

void CustomEmojiManager::repaintLater(
		not_null<Ui::CustomEmoji::Instance*> instance,
		Ui::CustomEmoji::RepaintRequest request) {
	auto &bunch = _repaints[request.duration];
	if (bunch.when < request.when) {
		bunch.when = request.when;
	}
	bunch.instances.emplace_back(instance);
	scheduleRepaintTimer();
}

void CustomEmojiManager::scheduleRepaintTimer() {
	if (_repaintTimerScheduled) {
		return;
	}
	_repaintTimerScheduled = true;
	Ui::PostponeCall(this, [=] {
		_repaintTimerScheduled = false;

		auto next = crl::time();
		for (const auto &[duration, bunch] : _repaints) {
			if (!next || next > bunch.when) {
				next = bunch.when;
			}
		}
		if (next && (!_repaintNext || _repaintNext > next)) {
			const auto now = crl::now();
			if (now >= next) {
				_repaintNext = 0;
				_repaintTimer.cancel();
				invokeRepaints();
			} else {
				_repaintNext = next;
				_repaintTimer.callOnce(next - now);
			}
		}
	});
}

void CustomEmojiManager::invokeRepaints() {
	_repaintNext = 0;
	const auto now = crl::now();
	auto repaint = std::vector<base::weak_ptr<Ui::CustomEmoji::Instance>>();
	for (auto i = begin(_repaints); i != end(_repaints);) {
		if (i->second.when > now) {
			++i;
			continue;
		}
		auto &list = i->second.instances;
		if (repaint.empty()) {
			repaint = std::move(list);
		} else {
			repaint.insert(
				end(repaint),
				std::make_move_iterator(begin(list)),
				std::make_move_iterator(end(list)));
		}
		i = _repaints.erase(i);
	}
	for (const auto &weak : repaint) {
		if (const auto strong = weak.get()) {
			strong->repaint();
		}
	}
	scheduleRepaintTimer();
}

Main::Session &CustomEmojiManager::session() const {
	return _owner->session();
}

Session &CustomEmojiManager::owner() const {
	return *_owner;
}

uint64 CustomEmojiManager::coloredSetId() const {
	return _coloredSetId;
}

int FrameSizeFromTag(SizeTag tag) {
	const auto emoji = EmojiSizeFromTag(tag);
	const auto factor = style::DevicePixelRatio();
	return Ui::Text::AdjustCustomEmojiSize(emoji / factor) * factor;
}

QString SerializeCustomEmojiId(const CustomEmojiId &id) {
	return QString::number(id.id);
}

QString SerializeCustomEmojiId(not_null<DocumentData*> document) {
	return SerializeCustomEmojiId({
		.id = document->id,
	});
}

CustomEmojiId ParseCustomEmojiData(QStringView data) {
	return { .id = data.toULongLong() };
}

bool AllowEmojiWithoutPremium(not_null<PeerData*> peer) {
	return peer->isSelf();
}

void InsertCustomEmoji(
		not_null<Ui::InputField*> field,
		not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	if (!sticker || sticker->alt.isEmpty()) {
		return;
	}
	Ui::InsertCustomEmojiAtCursor(
		field->textCursor(),
		sticker->alt,
		Ui::InputField::CustomEmojiLink(SerializeCustomEmojiId(document)));
}

Ui::Text::CustomEmojiFactory ReactedMenuFactory(
		not_null<Main::Session*> session) {
	return [owner = &session->data()](
			QStringView data,
			Fn<void()> repaint) -> std::unique_ptr<Ui::Text::CustomEmoji> {
		const auto prefix = u"default:"_q;
		if (data.startsWith(prefix)) {
			const auto &list = owner->reactions().list(
				Data::Reactions::Type::All);
			const auto emoji = data.mid(prefix.size()).toString();
			const auto id = Data::ReactionId{ { emoji } };
			const auto i = ranges::find(list, id, &Data::Reaction::id);
			if (i != end(list)) {
				const auto document = i->centerIcon
					? not_null(i->centerIcon)
					: i->selectAnimation;
				const auto size = st::emojiSize * (i->centerIcon ? 2 : 1);
				const auto tag = Data::CustomEmojiManager::SizeTag::Normal;
				const auto ratio = style::DevicePixelRatio();
				const auto skip = (Data::FrameSizeFromTag(tag) / ratio - size) / 2;
				return std::make_unique<Ui::Text::FirstFrameEmoji>(
					std::make_unique<Ui::Text::ShiftedEmoji>(
						owner->customEmojiManager().create(
							document,
							std::move(repaint),
							tag,
							size),
						QPoint(skip, skip)));
			}
		}
		return owner->customEmojiManager().create(data, std::move(repaint));
	};
}

} // namespace Data
