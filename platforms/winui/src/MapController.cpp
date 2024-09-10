#include "MapController.h"
#include "MapController.g.cpp"
#include "MapData.h"
#include "Marker.h"
#include "Renderer.h"
#include "Tangram.h"
#include "TangramPlatform.h"

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Storage.Streams.h>

using winrt::Microsoft::UI::Xaml::SizeChangedEventArgs;
using winrt::Microsoft::UI::Xaml::SizeChangedEventHandler;
using WinRTMarkerImpl = winrt::TangramWinUI::implementation::Marker;
using WinRTMapDataImpl = winrt::TangramWinUI::implementation::MapData;

namespace winrt::TangramWinUI::implementation {

MapController::~MapController() {
    assert(IsDestroying());
}

MapController::MapController()
    : m_fileDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread()),
      m_renderDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread()),
      m_workDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread()) {
}

MapController::MapController(SwapChainPanel panel, array_view<const hstring>& fontPaths) : MapController() {
    m_tileSources = multi_threaded_map<hstring, WinRTMapData>();
    m_markers = multi_threaded_map<uint32_t, WinRTMarker>();
    m_panel = panel;

    // when we increase the DPI of the display, above 1.5 it just looks very blurry with raster tiles,
    // so we limit the pixel scale
    constexpr static float MAX_PIXEL_SCALE = 1.5;

    // we create the context on the ui thread!
    m_renderer = std::make_unique<::TangramWinUI::Renderer>(*this);
    m_uiDispatcherQueue = DispatcherQueue::GetForCurrentThread();
    assert(m_uiDispatcherQueue);
    auto platform = new ::TangramWinUI::TangramPlatform(*this, Tangram::UrlClient::Options{}, fontPaths);
    m_map = std::make_unique<Tangram::Map>(std::unique_ptr<::Tangram::Platform>(platform));
    m_map->setSceneReadyListener([this](Tangram::SceneID id, auto) { m_onSceneLoaded(*this, id); });
    m_renderer->InitRendererOnUiThread(m_panel);

    m_panel.SizeChanged(SizeChangedEventHandler([this](auto, const SizeChangedEventArgs& e) {
        {
            std::scoped_lock mapLock(m_mapMutex);
            if (IsShuttingDown()) return;

            // TODO: Preserve some width,height buffer in the native viewport to save some very marginal size changes
            // Have a bit of extra "padding" then when we increase the size we still might have enough viewport size, so
            // we do not need to resize. When we are down-sizing can have the same padding to not call resize until we
            // go very low, just keep the current viewport.

            auto width = static_cast<int>(m_panel.ActualWidth());
            auto height = static_cast<int>(m_panel.ActualHeight());
            auto newPixelScale = std::min(MAX_PIXEL_SCALE, static_cast<float>(m_panel.XamlRoot().RasterizationScale()));
            auto oldPixelScale = m_map->getPixelScale();

            auto changed = false;

            std::scoped_lock resizeLock(m_resizeMutex);

            if (m_map->getViewportWidth() != width || height != m_map->getViewportHeight()) {
                m_newWidth = width;
                m_newHeight = height;
                changed = true;
            }

            if (fabs(newPixelScale - oldPixelScale) >= 0.01) {
                m_newPixelScale = newPixelScale;
                changed = true;
            }

            if (changed) m_resizeRequestedAt = std::chrono::high_resolution_clock::now();
        }
    }));

    m_newWidth = static_cast<int>(m_panel.ActualWidth());
    m_newHeight = static_cast<int>(m_panel.ActualHeight());
    m_newPixelScale = std::min(MAX_PIXEL_SCALE, static_cast<float>(m_panel.XamlRoot().RasterizationScale()));

    m_map->setupGL();

    m_renderDispatcherQueueController.DispatcherQueue().TryEnqueue(DispatcherQueuePriority::Normal,
                                                                   DispatcherQueueHandler([this] { RenderThread(); }));
}

event_token MapController::OnLoaded(EventHandler<WinRTMapController> const& handler) {
    auto token =  m_onLoaded.add(handler);
    bool isLoaded;
    {
        std::scoped_lock lock(m_mapMutex);
        isLoaded = m_loaded;
    }

    if (isLoaded)
        ScheduleOnUIThread([this]{ m_onLoaded(m_panel, *this); });

    return token;
}

void MapController::OnLoaded(event_token const& token) noexcept { m_onLoaded.remove(token); }

event_token MapController::OnSceneLoaded(EventHandler<uint32_t> const& handler) { return m_onSceneLoaded.add(handler); }

void MapController::OnSceneLoaded(event_token const& token) noexcept { m_onSceneLoaded.remove(token); }

event_token MapController::OnRegionWillChange(EventHandler<bool> const& handler) {
    return m_onRegionWillChange.add(handler);
}

void MapController::OnRegionWillChange(event_token const& token) noexcept { m_onRegionWillChange.remove(token); }

event_token MapController::OnRegionDidChange(EventHandler<bool> const& handler) {
    return m_onRegionDidChange.add(handler);
}

void MapController::OnRegionDidChange(event_token const& token) noexcept { m_onRegionDidChange.remove(token); }

event_token MapController::OnRegionIsChanging(EventHandler<int> const& handler) {
    return m_onRegionIsChanging.add(handler);
}

void MapController::OnRegionIsChanging(event_token const& token) noexcept { m_onRegionIsChanging.remove(token); }

event_token MapController::OnViewComplete(EventHandler<int> const& handler) { return m_onViewComplete.add(handler); }

void MapController::OnViewComplete(event_token const& token) noexcept { m_onViewComplete.remove(token); }

void MapController::UseCachedGlState(bool useCache) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->useCachedGlState(useCache);
}

void MapController::HandlePanGesture(float startX, float startY, float endX, float endY) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::JUMPING);
    m_map->handlePanGesture(startX, startY, endX, endY);
}

void MapController::HandlePinchGesture(float x, float y, float scale, float velocity) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::JUMPING);
    m_map->handlePinchGesture(x, y, scale, velocity);
}

void MapController::HandleShoveGesture(float distance) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::JUMPING);
    m_map->handleShoveGesture(distance);
}

void MapController::HandleFlingGesture(float x, float y, float velocityX, float velocityY) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::JUMPING);
    m_map->handleFlingGesture(x, y, velocityX, velocityY);
}

void MapController::HandleRotateGesture(float x, float y, float angle) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::JUMPING);
    m_map->handleRotateGesture(x, y, angle);
}

void MapController::HandleDoubleTapGesture(float x, float y) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->handleDoubleTapGesture(x, y);
}

void MapController::HandleTapGesture(float x, float y) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->handleTapGesture(x, y);
}

void MapController::HandlePanPinchRotateFlingShove(float panStartX, float panStartY, float panEndX, float panEndY,
                                                   float pinchPosX, float pinchPosY, float pinchValue,
                                                   float pinchVelocity, float rotPosX, float rotPosY, float rotRadians,
                                                   float flingPosX, float flingPosY, float flingVelocityX,
                                                   float flingVelocityY, float shoveDistance) {

    m_map->handlePanPinchRotateFlingShove(panStartX, panStartY, panEndX, panEndY, pinchPosX, pinchPosY, pinchValue,
                                          pinchVelocity, rotPosX, rotPosY, rotRadians, flingPosX, flingPosY,
                                          flingVelocityX, flingVelocityY, shoveDistance);
}

void MapController::CaptureFrame(EventHandler<SoftwareBitmap> callback) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_renderer->CaptureFrame([this, callback](SoftwareBitmap bitmap) { callback(m_panel, bitmap); });
}

IAsyncAction MapController::Shutdown() {
    m_shuttingDown = true;

    std::scoped_lock lock(m_mapMutex);

    m_onRegionIsChanging.clear();
    m_onRegionWillChange.clear();
    m_onRegionDidChange.clear();
    m_onViewComplete.clear();
    m_onSceneLoaded.clear();
    m_onLoaded.clear();

    co_await m_renderDispatcherQueueController.ShutdownQueueAsync();
    co_await m_workDispatcherQueueController.ShutdownQueueAsync();
    co_await m_fileDispatcherQueueController.ShutdownQueueAsync();

    m_renderDispatcherQueueController = nullptr;
    m_workDispatcherQueueController = nullptr;
    m_fileDispatcherQueueController = nullptr;

    m_markerPickHandler = nullptr;
    m_featurePickHandler = nullptr;
    m_labelPickHandler = nullptr;

    for (auto& t : m_tileSources) {
        auto nativeMapData = winrt::get_self<WinRTMapDataImpl>(t.Value());
        m_map->removeTileSource(*nativeMapData->Source());
        nativeMapData->Invalidate();
    }

    m_tileSources.Clear();

    m_map->markerRemoveAll();
    for (auto& marker : m_markers) { winrt::get_self<WinRTMarkerImpl>(marker.Value())->Invalidate(); }
    m_markers.Clear();
    m_map.reset();

    // destroy the context from the main thread!
    if (m_uiDispatcherQueue.HasThreadAccess()) { m_renderer->Destroy(); } else {
        bool success = m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority::High, [this] { m_renderer->Destroy(); });
        assert(success);
    }
    m_uiDispatcherQueue = nullptr;
}

void MapController::SetPickRadius(float radius) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->setPickRadius(radius);
}

void MapController::PickMarker(float posX, float posY, int identifier) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    if (!m_markerPickHandler) return;

    m_map->pickMarkerAt(posX, posY, identifier, [this](const Tangram::MarkerPickResult* pick) {
        if (!m_markerPickHandler) return;

        if (pick == nullptr) {
            m_markerPickHandler(m_panel, nullptr);
            return;
        }

        auto result = PickResult();
        result.MarkerId(pick->id);
        result.Position(LngLat(pick->coordinates.longitude, pick->coordinates.latitude));
        result.Identifier(pick->identifier);
        m_markerPickHandler(*this, result);
    });
}

void MapController::PickFeature(float posX, float posY, int identifier) {
    std::scoped_lock mapLock(m_mapMutex);
    if (!m_featurePickHandler || IsShuttingDown()) return;

    m_map->pickFeatureAt(posX, posY, identifier, [this](const Tangram::FeaturePickResult* pick) {
        if (!m_featurePickHandler) return;

        if (pick == nullptr) {
            m_featurePickHandler(m_panel, nullptr);
            return;
        }
        auto result = PickResult();
        result.Position(LngLat(pick->position[0], pick->position[1]));
        result.Identifier(pick->identifier);
        m_featurePickHandler(*this, result);
    });
}

void MapController::PickLabel(float posX, float posY, int identifier) {
    std::scoped_lock mapLock(m_mapMutex);
    if (!m_labelPickHandler || IsShuttingDown()) return;

    m_map->pickLabelAt(posX, posY, identifier, [this](const Tangram::LabelPickResult* pick) {
        if (!m_labelPickHandler) return;

        if (pick == nullptr) {
            m_labelPickHandler(m_panel, nullptr);
            return;
        }
        auto result = PickResult();
        result.FeatureId(pick->type);
        result.Position(LngLat(pick->coordinates.longitude, pick->coordinates.latitude));
        result.Identifier(pick->identifier);
        m_labelPickHandler(*this, result);
    });
}

CameraPosition MapController::GetCameraPosition() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    auto pos = m_map->getCameraPosition();
    auto cam = CameraPosition();
    cam.Tilt(pos.tilt);
    cam.Zoom(pos.zoom);
    cam.Rotation(pos.rotation);
    cam.Latitude(pos.latitude);
    cam.Longitude(pos.longitude);
    return cam;
}

void MapController::UpdateCameraPosition(const CameraPosition& cameraPosition, int duration) {
    Tangram::CameraUpdate update{};
    update.set = Tangram::CameraUpdate::set_tilt |
                 Tangram::CameraUpdate::set_zoom |
                 Tangram::CameraUpdate::set_lnglat|
                 Tangram::CameraUpdate::set_rotation;

    update.lngLat = Tangram::LngLat(cameraPosition.Longitude(), cameraPosition.Latitude());
    update.zoom = cameraPosition.Zoom();
    update.rotation = cameraPosition.Rotation();
    update.tilt = cameraPosition.Tilt();

    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    if (duration > 0) { SetMapRegionState(MapRegionChangeState::ANIMATING); } else {
        SetMapRegionState(MapRegionChangeState::JUMPING);
    }

    m_map->updateCameraPosition(update, duration / 1000.f);
}

void MapController::UpdateCameraPosition(LngLat sw, LngLat ne, int paddingLeft, int paddingTop, int paddingRight,
                                         int paddingBottom, int duration) {
    Tangram::CameraUpdate update{};
    update.set = Tangram::CameraUpdate::Flags::set_bounds;
    update.bounds[0] = {sw.Longitude(), sw.Latitude()};
    update.bounds[1] = {ne.Longitude(), ne.Latitude()};
    update.padding = {paddingLeft, paddingTop, paddingRight, paddingBottom};

    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    if (duration > 0) { SetMapRegionState(MapRegionChangeState::ANIMATING); } else {
        SetMapRegionState(MapRegionChangeState::JUMPING);
    }

    m_map->updateCameraPosition(update, duration / 1000.f);
}

void MapController::SetMapRegionState(MapRegionChangeState state) {
    auto prevState = m_currentState;
    m_currentState = state;

    if (prevState == MapRegionChangeState::IDLE) {
        if (state != MapRegionChangeState::IDLE && m_onRegionWillChange) {
            auto isAnimating = state == MapRegionChangeState::ANIMATING;
            ScheduleOnWorkThread([this, isAnimating] { m_onRegionWillChange(m_panel, isAnimating); });
        }

        return;
    }

    if (prevState == MapRegionChangeState::JUMPING) {
        if (state == MapRegionChangeState::IDLE && m_onRegionDidChange) {
            ScheduleOnWorkThread([this] { m_onRegionDidChange(m_panel, false); });
        } else if (state == MapRegionChangeState::JUMPING && m_onRegionIsChanging) {
            ScheduleOnWorkThread([this] { m_onRegionIsChanging(m_panel, 0); });
        }
        return;
    }

    if (prevState == MapRegionChangeState::ANIMATING) {
        if (state == MapRegionChangeState::IDLE && m_onRegionDidChange) {
            ScheduleOnWorkThread([this] { m_onRegionDidChange(m_panel, true); });
        } else if (state == MapRegionChangeState::ANIMATING && m_onRegionIsChanging) {
            ScheduleOnWorkThread([this] { m_onRegionIsChanging(m_panel, 0); });
        }
    }
}

void MapController::RenderThread() {
    {
        std::scoped_lock lock(m_mapMutex);
        m_renderer->MakeActive();

        std::scoped_lock resizeLock(m_resizeMutex);
        m_map->setPixelScale(m_newPixelScale);
        m_map->resize(m_newWidth, m_newHeight);
        m_resizeRequestedAt = {};
        m_newHeight = 0;
        m_newWidth = 0;
        m_newPixelScale = 0;

        m_loaded = true;
    }

    ScheduleOnUIThread([this]() { m_onLoaded(m_panel, *this); });

    uint64_t lastThreadRedrawId = -1;

    while (!IsShuttingDown()) {
        const auto currentRequestId = m_renderRequestId.load();
        auto resizing = false;

        if (m_resizeMutex.try_lock()) {
            if (m_resizeRequestedAt.has_value()) {
                resizing = true;

                if (m_newWidth) {
                    m_map->resize(m_newWidth, m_newHeight);
                    m_newWidth = 0;
                    m_newHeight = 0;
                }

                if (m_newPixelScale > 0.01) {
                    m_map->setPixelScale(m_newPixelScale);
                    m_newPixelScale = 0;
                }

                // lets give the UI N-millisec for redrawing
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - m_resizeRequestedAt.value());

                if (diff.count() > 15) { m_resizeRequestedAt = {}; }
            }

            m_resizeMutex.unlock();
        }

        if(lastThreadRedrawId != currentRequestId || resizing) {
            lastThreadRedrawId = currentRequestId;
            m_renderer->Render();
        }
    }
}

void MapController::SetMapRegionStateIdle() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::IDLE);
}

void MapController::CancelCameraAnimation() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->cancelCameraAnimation();
}

void MapController::RaiseViewCompleteEvent() {
    if (IsShuttingDown()) return;
    if (m_onViewComplete) ScheduleOnWorkThread([this] { m_onViewComplete(m_panel, 0); });
}

LngLat MapController::ScreenPositionToLngLat(double x, double y) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    double olng, olat;
    if (m_map->screenPositionToLngLat(x, y, &olng, &olat)) { return LngLat(olng, olat); }

    return nullptr;
}

Point MapController::LngLatToScreenPosition(double lng, double lat, bool clipToViewPort) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return {};

    double x, y;
    if (m_map->lngLatToScreenPosition(lng, lat, &x, &y, clipToViewPort)) {
        return Point{static_cast<float>(x), static_cast<float>(y)};
    }

    return {};
}

void MapController::SetLayer(const hstring& layerName, const hstring& style) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;
    m_map->setLayer(to_string(layerName), to_string(style));
}

void MapController::SetTileSourceUrl(const hstring& sourceName, const hstring& url) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;
    m_map->setTileSourceUrl(to_string(sourceName), to_string(url));
}

hstring MapController::GetTileSourceUrl(const hstring& sourceName) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return {};
    return to_hstring(m_map->getTileSourceUrl(to_string(sourceName)));
}

int MapController::LoadSceneYaml(const hstring& yaml, const hstring& resourceRoot, bool loadAsync) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return -1;

    Tangram::SceneOptions options{};
    options.numTileWorkers = 2;
    options.memoryTileCacheSize = Tangram::SceneOptions::DEFAULT_CACHE_SIZE * 2;
    options.url = Tangram::Url(to_string(resourceRoot));
    options.yaml = to_string(yaml);
    return m_map->loadScene(std::move(options), loadAsync);
}

bool MapController::LayerExists(const hstring& layerName) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return false;
    return m_map->layerExists(to_string(layerName));
}

WinRTMapData MapController::AddDataLayer(const hstring& layerName) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    if (m_tileSources.HasKey(layerName)) {
        return m_tileSources.Lookup(layerName);
    }

    auto source = std::make_shared<Tangram::ClientDataSource>(m_map->getPlatform(), to_string(layerName), "");

    auto mapData = WinRTMapData();
    auto native = winrt::get_self<WinRTMapDataImpl>(mapData);
    native->Init(source.get(), this);

    (void)m_tileSources.Insert(layerName, mapData);

    m_map->addTileSource(source);
    return mapData;
}

void MapController::RemoveDataLayer(const WinRTMapData& mapData) {
    // at this point the map controller is locked by MapData !
    if (!m_tileSources.HasKey(mapData.Name())) return;
    m_tileSources.Remove(mapData.Name());

    const auto nativeMapData = winrt::get_self<WinRTMapDataImpl>(mapData);
    if (auto nativeSource = nativeMapData->Source()) { m_map->removeTileSource(*nativeSource); }
}

void MapController::RemoveMarker(const WinRTMarker& marker) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerRemove(marker.Id());
    m_markers.Remove(marker.Id());
    get_self<WinRTMarkerImpl>(marker)->Invalidate();
}

TangramWinUI::Marker MapController::AddMarker() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    auto marker = WinRTMarker();
    auto nativeMarker = winrt::get_self<WinRTMarkerImpl>(marker);
    auto id = m_map->markerAdd();
    nativeMarker->Init(id, this);

    (void)m_markers.Insert(id, marker);
    return marker;
}

void MapController::RemoveAllMarkers() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerRemoveAll();
    for (auto& marker : m_markers) { winrt::get_self<WinRTMarkerImpl>(marker.Value())->Invalidate(); }
    m_markers.Clear();
}

void MapController::SetMaxZoomLevel(float maxZoomLevel) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->setMaxZoom(maxZoomLevel);
}

float MapController::GetPixelsPerMeter() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return 0;

    return m_map->pixelsPerMeter();
}

void MapController::SetRotationAngle(float angle) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    return m_map->setRotation(angle);
}

float MapController::GetRotationAngle() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return 0;

    return m_map->getRotation();
}

float MapController::GetZoomVelocity() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return 0.f;

    return m_map->getZoomVelocity();
}

void MapController::SetMarkerPickHandler(EventHandler<PickResult> const& handler) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_markerPickHandler = handler;
}

void MapController::SetFeaturePickHandler(EventHandler<PickResult> const& handler) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_featurePickHandler = handler;
}

void MapController::SetLabelPickHandler(EventHandler<PickResult> const& handler) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_labelPickHandler = handler;
}

void MapController::RequestRender() {
    m_renderRequestId.fetch_add(1);
}

} // namespace winrt::TangramWinUI::implementation