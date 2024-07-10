#include "MapController.h"
#include "MapController.g.cpp"
#include "MapData.h"
#include "Marker.h"
#include "Renderer.h"
#include "Tangram.h"
#include "TangramPlatform.h"

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Microsoft.UI.Dispatching.h>

using winrt::Microsoft::UI::Xaml::SizeChangedEventArgs;
using winrt::Microsoft::UI::Xaml::SizeChangedEventHandler;
using NativeMarker = winrt::TangramWinUI::implementation::Marker;

namespace winrt::TangramWinUI::implementation {

MapController::~MapController() {
    assert(IsDestroying());
}

MapController::MapController()
    : m_renderDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread()),
      m_workDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread()),
      m_fileDispatcherQueueController(DispatcherQueueController::CreateOnDedicatedThread())
{
    m_tileSources = multi_threaded_map<hstring, RuntimeMapData>();
    m_markers = multi_threaded_map<uint32_t, RuntimeMarker>();
}

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

void MapController::Init(SwapChainPanel panel) {
    {
        std::scoped_lock lock(m_mapMutex);
        m_uiDispatcherQueue = DispatcherQueue::GetForCurrentThread();
        assert(m_uiDispatcherQueue);
        m_panel = panel;
        // we create the context on the ui thread!
        m_renderer = std::make_unique<::TangramWinUI::Renderer>(*this);
        auto platform = new ::TangramWinUI::TangramPlatform(*this, Tangram::UrlClient::Options{});
        m_map = std::make_shared<Tangram::Map>(std::unique_ptr<::Tangram::Platform>(platform));

        m_map->setSceneReadyListener([this](Tangram::SceneID id, auto) { m_onSceneLoaded(*this, id); });

        m_renderer->InitRendererOnUiThread(panel);

        panel.SizeChanged(SizeChangedEventHandler([this](auto, const SizeChangedEventArgs& e) {
            {
                std::scoped_lock mapLock(m_mapMutex);
                // we must do it on the UI thread, where we initialized our context
                m_map->resize(static_cast<int>(e.NewSize().Width), static_cast<int>(e.NewSize().Height));
            }

            // request render twice to surely work
            RequestRender(true);
            RequestRender(true);
        }));
    }

    auto width = static_cast<int>(panel.ActualWidth());
    auto height = static_cast<int>(panel.ActualHeight());
    auto density = static_cast<float>(panel.XamlRoot().RasterizationScale());

    ScheduleOnRenderThread([this, density, width, height] {
        {
            if (IsShuttingDown()) return;
            std::scoped_lock rendererRenderLock(m_renderer->Mutex());
            if (IsShuttingDown()) return;

            m_renderer->MakeActive();
            m_map->setPixelScale(density);
            m_map->setupGL();
            m_map->resize(width, height);
        }

        RequestRender(true);
    });
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

void MapController::CaptureFrame(EventHandler<SoftwareBitmap> callback) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_renderer->CaptureFrame([this, callback](SoftwareBitmap bitmap) { callback(m_panel, bitmap); });
}

IAsyncAction MapController::Shutdown() {
    m_shuttingDown = true;
    std::scoped_lock lock(m_mapMutex, m_renderMutex, m_workMutex, m_renderer->Mutex());
    m_onRegionIsChanging.clear();
    m_onRegionWillChange.clear();
    m_onRegionDidChange.clear();
    m_onViewComplete.clear();
    m_onSceneLoaded.clear();

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
        auto nativeMapData = winrt::get_self<NativeMapData>(t.Value());
        m_map->removeTileSource(*nativeMapData->Source());
        nativeMapData->Invalidate();
    }

    m_tileSources.Clear();

    m_map->markerRemoveAll();
    for (auto& marker : m_markers) { winrt::get_self<NativeMarker>(marker.Value())->Invalidate(); }
    m_markers.Clear();

    m_map->shutdown();
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


void MapController::UpdateCameraPosition(LngLat sw, LngLat ne, int paddingLeft, int paddingTop, int paddingRight,
                                         int paddingBottom, float duration) {

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

    m_map->updateCameraPosition(update, duration);
}

void MapController::UpdateCameraPosition(LngLat lngLat, float duration) {
    Tangram::CameraUpdate update{};
    update.set = Tangram::CameraUpdate::Flags::set_lnglat;
    update.lngLat = Tangram::LngLat(lngLat.Longitude(), lngLat.Latitude());

    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    if (duration > 0) { SetMapRegionState(MapRegionChangeState::ANIMATING); } else {
        SetMapRegionState(MapRegionChangeState::JUMPING);
    }
    m_map->updateCameraPosition(update, duration);
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

        return;
    }
}

void MapController::MarkerSetVisible(MarkerID id, bool visible) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerSetVisible(id, visible);
}

void MapController::MarkerSetDrawOrder(MarkerID id, int drawOrder) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;
    m_map->markerSetDrawOrder(id, drawOrder);
}

void MapController::MarkerSetPointEased(MarkerID id, Tangram::LngLat lngLat, float duration, Tangram::EaseType ease) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerSetPointEased(id, lngLat, duration, ease);
}

void MapController::MarkerSetPoint(MarkerID id, Tangram::LngLat lngLat) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerSetPoint(id, lngLat);
}

void MapController::MarkerSetBitmap(MarkerID id, int width, int height, uint32_t* data) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerSetBitmap(id, width, height, data);
}

void MapController::MarkerSetStylingFromString(MarkerID id, const char* styling) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerSetStylingFromString(id, styling);
}

void MapController::LayerGenerateTiles(NativeMapData& data) {
    {
        std::scoped_lock mapLock(m_mapMutex);
        if (IsShuttingDown()) return;

        m_map->clearTileCache(data.Id());
    }
    data.Source()->generateTiles();
    RequestRender();
}

void MapController::SetMapRegionStateIdle() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    SetMapRegionState(MapRegionChangeState::IDLE);
}

void MapController::RaiseViewCompleteEvent() {
    if (IsShuttingDown()) return;
    if (m_onViewComplete) ScheduleOnWorkThread([this] { m_onViewComplete(m_panel, 0); });
}

LngLat MapController::ScreenPositionToLngLat(double lng, double lat) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    double olng, olat;
    if (m_map->screenPositionToLngLat(lng, lat, &olng, &olat)) { return LngLat(olng, olat); }

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

int MapController::LoadSceneYaml(const hstring& yaml, const hstring& resourceRoot) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return -1;

    Tangram::SceneOptions options{};
    options.numTileWorkers = 2;
    options.prefetchTiles = true;
    options.memoryTileCacheSize = static_cast<uint32_t>(64 * 1024 * 1024);
    options.url = Tangram::Url(to_string(resourceRoot));
    options.yaml = to_string(yaml);
    return m_map->loadScene(std::move(options), true);
}

bool MapController::LayerExists(const hstring& layerName) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return false;
    return m_map->layerExists(to_string(layerName));
}

RuntimeMapData MapController::AddDataLayer(const hstring& layerName) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    if (m_tileSources.HasKey(layerName)) { return m_tileSources.Lookup(layerName); }
    auto source = std::make_shared<Tangram::ClientDataSource>(m_map->getPlatform(), to_string(layerName), "");

    auto mapData = RuntimeMapData();
    auto native = winrt::get_self<NativeMapData>(mapData);
    native->Name(layerName);
    native->SetSource(source.get());
    native->SetController(this);
    (void)m_tileSources.Insert(layerName, mapData);

    m_map->addTileSource(source);
    return mapData;
}

void MapController::RemoveDataLayer(const RuntimeMapData& mapData) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    if (!m_tileSources.HasKey(mapData.Name())) return;
    m_tileSources.Remove(mapData.Name());

    const auto nativeMapData = winrt::get_self<NativeMapData>(mapData);

    if (auto nativeSource = nativeMapData->Source()) { m_map->removeTileSource(*nativeSource); }

    nativeMapData->Invalidate();
}

void MapController::RemoveMarker(const RuntimeMarker& marker) {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerRemove(marker.Id());
    m_markers.Remove(marker.Id());
    get_self<NativeMarker>(marker)->Invalidate();
}

TangramWinUI::Marker MapController::AddMarker() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return nullptr;

    auto marker = RuntimeMarker();
    auto nativeMarker = winrt::get_self<NativeMarker>(marker);
    auto id = m_map->markerAdd();
    nativeMarker->Id(id);
    nativeMarker->SetController(this);
    (void)m_markers.Insert(id, marker);
    return marker;
}

void MapController::RemoveAllMarkers() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return;

    m_map->markerRemoveAll();
    for (auto& marker : m_markers) { winrt::get_self<NativeMarker>(marker.Value())->Invalidate(); }
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

float MapController::GetRotationAngle() {
    std::scoped_lock mapLock(m_mapMutex);
    if (IsShuttingDown()) return 0;

    return m_map->getRotation();
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

void MapController::RequestRender(bool forced) {
    if (IsShuttingDown()) return;
    // we allow only 2 render requests to be present ( 1 is always being processed, so it is really just queues a
    // next one)
    if (forced || m_queuedRenderRequests.load() < 2) ScheduleOnRenderThread([this] { m_renderer->Render(); });
}

} // namespace winrt::TangramWinUI::implementation