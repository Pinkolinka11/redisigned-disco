/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "history/view/media/history_view_file.h"

enum class ImageRoundRadius;

namespace Data {
class PhotoMedia;
} // namespace Data

namespace Media {
namespace Streaming {
class Instance;
struct Update;
enum class Error;
struct Information;
} // namespace Streaming
} // namespace Media

namespace HistoryView {

class Photo final : public File {
public:
	Photo(
		not_null<Element*> parent,
		not_null<HistoryItem*> realParent,
		not_null<PhotoData*> photo);
	Photo(
		not_null<Element*> parent,
		not_null<PeerData*> chat,
		not_null<PhotoData*> photo,
		int width);
	~Photo();

	void draw(Painter &p, const PaintContext &context) const override;
	TextState textState(QPoint point, StateRequest request) const override;

	[[nodiscard]] TextSelection adjustSelection(
			TextSelection selection,
			TextSelectType type) const override {
		return _caption.adjustSelection(selection, type);
	}
	uint16 fullSelectionLength() const override {
		return _caption.length();
	}
	bool hasTextForCopy() const override {
		return !_caption.isEmpty();
	}

	TextForMimeData selectedText(TextSelection selection) const override;

	PhotoData *getPhoto() const override {
		return _data;
	}

	QSize sizeForGroupingOptimal(int maxWidth) const override;
	QSize sizeForGrouping(int width) const override;
	void drawGrouped(
		Painter &p,
		const PaintContext &context,
		const QRect &geometry,
		RectParts sides,
		RectParts corners,
		float64 highlightOpacity,
		not_null<uint64*> cacheKey,
		not_null<QPixmap*> cache) const override;
	TextState getStateGrouped(
		const QRect &geometry,
		RectParts sides,
		QPoint point,
		StateRequest request) const override;

	TextWithEntities getCaption() const override {
		return _caption.toTextWithEntities();
	}
	bool needsBubble() const override;
	bool customInfoLayout() const override {
		return _caption.isEmpty();
	}
	QPoint resolveCustomInfoRightBottom() const override;
	bool skipBubbleTail() const override {
		return isRoundedInBubbleBottom() && _caption.isEmpty();
	}
	bool isReadyForOpen() const override;

	void parentTextUpdated() override;

	bool hasHeavyPart() const override;
	void unloadHeavyPart() override;

protected:
	float64 dataProgress() const override;
	bool dataFinished() const override;
	bool dataLoaded() const override;

private:
	struct Streamed;

	void showPhoto(FullMsgId id);

	void create(FullMsgId contextId, PeerData *chat = nullptr);

	void playAnimation(bool autoplay) override;
	void stopAnimation() override;
	void checkAnimation() override;

	void ensureDataMediaCreated() const;
	void dataMediaCreated() const;

	QSize countOptimalSize() override;
	QSize countCurrentSize(int newWidth) override;
	[[nodiscard]] int adjustHeightForLessCrop(
		QSize dimensions,
		QSize current) const;

	bool needInfoDisplay() const;
	void validateGroupedCache(
		const QRect &geometry,
		RectParts corners,
		not_null<uint64*> cacheKey,
		not_null<QPixmap*> cache) const;
	void validateImageCache(
		QSize outer,
		ImageRoundRadius radius,
		RectParts corners) const;
	[[nodiscard]] QImage prepareImageCache(
		QSize outer,
		ImageRoundRadius radius,
		RectParts corners) const;
	[[nodiscard]] QImage prepareImageCache(QSize outer) const;

	bool videoAutoplayEnabled() const;
	void setStreamed(std::unique_ptr<Streamed> value);
	void repaintStreamedContent();
	void checkStreamedIsStarted() const;
	bool createStreamingObjects();
	void handleStreamingUpdate(::Media::Streaming::Update &&update);
	void handleStreamingError(::Media::Streaming::Error &&error);
	void streamingReady(::Media::Streaming::Information &&info);
	void paintUserpicFrame(
		Painter &p,
		const PaintContext &context,
		QPoint photoPosition) const;

	const not_null<PhotoData*> _data;
	Ui::Text::String _caption;
	mutable std::shared_ptr<Data::PhotoMedia> _dataMedia;
	mutable std::unique_ptr<Streamed> _streamed;
	mutable QImage _imageCache;
	int _serviceWidth = 0;
	mutable int _imageCacheRoundRadius : 4 = 0;
	mutable int _imageCacheRoundCorners : 12 = 0;
	mutable int _imageCacheBlurred : 1 = 0;

};

} // namespace HistoryView
