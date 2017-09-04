#include "android_renderer_frontend.hpp"

#include <mbgl/actor/scheduler.hpp>
#include <mbgl/renderer/backend_scope.hpp>
#include <mbgl/renderer/renderer.hpp>
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

AndroidRendererFrontend::AndroidRendererFrontend(float pixelRatio,
                                                 FileSource& fileSource,
                                                 Scheduler& scheduler,
                                                 std::string programCacheDir,
                                                 InvalidateCallback invalidate)
        : backend(std::make_unique<AndroidRendererBackend>())
        , glThreadCallback(std::make_unique<Actor<AndroidGLThread::InvalidateCallback>>(
                *Scheduler::GetCurrent(),
                [&]() {
                    Log::Info(Event::JNI, "GL Thread invalidate callback");
                    // TODO: replace the whole thing with rendererOberver.invalidate()?
                    asyncInvalidate.send();
                }
        ))
        , glThread(std::make_unique<AndroidGLThread>(
                glThreadCallback->self(),
                *backend,
                pixelRatio,
                fileSource,
                scheduler,
                GLContextMode::Unique,
                programCacheDir
        ))
        , asyncInvalidate([=, invalidate=std::move(invalidate)]() {
            invalidate();
        })
        , mapRunLoop(util::RunLoop::Get()) {
}

AndroidRendererFrontend::~AndroidRendererFrontend() = default;

void AndroidRendererFrontend::reset() {
    assert(glThread);
    glThread.reset();
}

void AndroidRendererFrontend::setObserver(RendererObserver& observer) {
    assert (glThread);
    assert (util::RunLoop::Get());
    rendererObserver = std::make_unique<ForwardingRendererObserver>(*mapRunLoop, observer);
    glThread->actor().invoke(&Renderer::setObserver, rendererObserver.get());
}

void AndroidRendererFrontend::update(std::shared_ptr<UpdateParameters> params) {
    updateParameters = std::move(params);
    asyncInvalidate.send();
}

// Called on OpenGL thread
void AndroidRendererFrontend::render() {
    assert (glThread);
    if (!updateParameters) return;

    // Process the gl thread mailbox
    glThread->process();

    // Activate the backend
    BackendScope backendGuard { *backend };

    // Ensure that the "current" scheduler on the render thread is
    // actually the scheduler from the orchestration  thread
    Scheduler::SetCurrent(glThread.get());

    if (framebufferSizeChanged) {
        backend->updateViewPort();
        framebufferSizeChanged = false;
    }

    glThread->renderer->render(*updateParameters);
}

void AndroidRendererFrontend::onLowMemory() {
    assert (glThread);
    glThread->actor().invoke(&Renderer::onLowMemory);
}

std::vector<Feature> AndroidRendererFrontend::querySourceFeatures(const std::string& sourceID,
                                                                  const SourceQueryOptions& options) const {
    assert (glThread);
    // Waits for the result from the orchestration thread and returns
    return glThread->actor().ask(&Renderer::querySourceFeatures, sourceID, options).get();
}

std::vector<Feature> AndroidRendererFrontend::queryRenderedFeatures(const ScreenBox& box,
                                                                    const RenderedQueryOptions& options) const {
    assert (glThread);

    // Select the right overloaded method
    std::vector<Feature> (Renderer::*fn)(const ScreenBox&, const RenderedQueryOptions&) const = &Renderer::queryRenderedFeatures;

    // Waits for the result from the orchestration thread and returns
    return glThread->actor().ask(fn, box, options).get();
}

std::vector<Feature> AndroidRendererFrontend::queryRenderedFeatures(const ScreenCoordinate& point,
                                                                    const RenderedQueryOptions& options) const {
    assert (glThread);

    // Select the right overloaded method
    std::vector<Feature> (Renderer::*fn)(const ScreenCoordinate&, const RenderedQueryOptions&) const = &Renderer::queryRenderedFeatures;

    // Waits for the result from the orchestration thread and returns
    return glThread->actor().ask(fn, point, options).get();
}

AnnotationIDs AndroidRendererFrontend::queryPointAnnotations(const ScreenBox& box) const {
    assert (glThread);

    // Waits for the result from the orchestration thread and returns
    return glThread->actor().ask(&Renderer::queryPointAnnotations, box).get();
}

void AndroidRendererFrontend::requestSnapshot(SnapshotCallback callback_) {
    snapshotCallback = std::make_unique<SnapshotCallback>([callback=std::move(callback_), runloop=util::RunLoop::Get()](PremultipliedImage image){
        runloop->invoke([&]() {
            callback(std::move(image));
        });
    });
}

void AndroidRendererFrontend::resizeFramebuffer(int width, int height) {
    backend->resizeFramebuffer(width, height);
    framebufferSizeChanged = true;
    // TODO: thread safe?
    asyncInvalidate.send();
}

} // namespace android
} // namespace mbgl

