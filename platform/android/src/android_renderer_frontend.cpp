#include "android_renderer_frontend.hpp"

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/renderer/renderer_observer.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/util/thread.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/logging.hpp>

#include "android_renderer_backend.hpp"

namespace mbgl {
namespace android {

// Forwards RendererObserver signals to the given
// Delegate RendererObserver on the given RunLoop
class ForwardingRendererObserver : public RendererObserver {
public:
    ForwardingRendererObserver(util::RunLoop& mapRunLoop_, RendererObserver& delegate_)
            : mapRunLoop(mapRunLoop_)
            , delegate(delegate_) {}

    void onInvalidate() override {
        mapRunLoop.invoke([&]() {
            delegate.onInvalidate();
        });
    }

    void onResourceError(std::exception_ptr err) override {
        mapRunLoop.invoke([&, err]() {
            delegate.onResourceError(err);
        });
    }

    void onWillStartRenderingMap() override {
        mapRunLoop.invoke([&]() {
            delegate.onWillStartRenderingMap();
        });
    }

    void onWillStartRenderingFrame() override {
        mapRunLoop.invoke([&]() {
            delegate.onWillStartRenderingFrame();
        });
    }

    void onDidFinishRenderingFrame(RenderMode mode, bool repaintNeeded) override {
        mapRunLoop.invoke([&, mode, repaintNeeded]() {
            delegate.onDidFinishRenderingFrame(mode, repaintNeeded);
        });
    }

    void onDidFinishRenderingMap() override {
        mapRunLoop.invoke([&]() {
            delegate.onDidFinishRenderingMap();
        });
    }

private:
    util::RunLoop& mapRunLoop;
    RendererObserver& delegate;
};

AndroidRendererFrontend::AndroidRendererFrontend(MapRenderer& mapRenderer_)
        : mapRenderer(mapRenderer_)
        , mapRunLoop(util::RunLoop::Get()) {
}

AndroidRendererFrontend::~AndroidRendererFrontend() = default;

void AndroidRendererFrontend::reset() {
    mapRenderer.reset();
    rendererObserver.reset();
}

void AndroidRendererFrontend::setObserver(RendererObserver& observer) {
    assert (util::RunLoop::Get());
    rendererObserver = std::make_unique<ForwardingRendererObserver>(*mapRunLoop, observer);
    mapRenderer.actor().invoke(&Renderer::setObserver, rendererObserver.get());
}

void AndroidRendererFrontend::update(std::shared_ptr<UpdateParameters> params) {
    mapRenderer.update(std::move(params));
    mapRenderer.requestRender();
}

void AndroidRendererFrontend::onLowMemory() {
    mapRenderer.actor().invoke(&Renderer::onLowMemory);
}

std::vector<Feature> AndroidRendererFrontend::querySourceFeatures(const std::string& sourceID,
                                                                  const SourceQueryOptions& options) const {
    // Waits for the result from the orchestration thread and returns
    return mapRenderer.actor().ask(&Renderer::querySourceFeatures, sourceID, options).get();
}

std::vector<Feature> AndroidRendererFrontend::queryRenderedFeatures(const ScreenBox& box,
                                                                    const RenderedQueryOptions& options) const {

    // Select the right overloaded method
    std::vector<Feature> (Renderer::*fn)(const ScreenBox&, const RenderedQueryOptions&) const = &Renderer::queryRenderedFeatures;

    // Waits for the result from the orchestration thread and returns
    return mapRenderer.actor().ask(fn, box, options).get();
}

std::vector<Feature> AndroidRendererFrontend::queryRenderedFeatures(const ScreenCoordinate& point,
                                                                    const RenderedQueryOptions& options) const {

    // Select the right overloaded method
    std::vector<Feature> (Renderer::*fn)(const ScreenCoordinate&, const RenderedQueryOptions&) const = &Renderer::queryRenderedFeatures;

    // Waits for the result from the orchestration thread and returns
    return mapRenderer.actor().ask(fn, point, options).get();
}

AnnotationIDs AndroidRendererFrontend::queryPointAnnotations(const ScreenBox& box) const {
    // Waits for the result from the orchestration thread and returns
    return mapRenderer.actor().ask(&Renderer::queryPointAnnotations, box).get();
}

void AndroidRendererFrontend::requestSnapshot(SnapshotCallback callback_) {
    snapshotCallback = std::make_unique<SnapshotCallback>([callback=std::move(callback_), runloop=util::RunLoop::Get()](PremultipliedImage image){
        runloop->invoke([&]() {
            callback(std::move(image));
        });
    });
}

} // namespace android
} // namespace mbgl

