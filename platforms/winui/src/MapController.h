#pragma once

#include "MapController.g.h"
#include "pch.h"
#include "tangram.h"
#include "Renderer.h"
#include <mutex>

namespace winrt::TangramWinUI::implementation {
    struct MapData;
    struct Marker;
}

using NativeMapData = winrt::TangramWinUI::implementation::MapData;
using Tangram::MarkerID;

namespace TangramWinUI {
class TangramPlatform;
} // namespace TangramWinUI

enum class MapRegionChangeState {
    IDLE,
    JUMPING,
    ANIMATING,
};

using winrt::Windows::Foundation::Point;
using RuntimeMapData = winrt::TangramWinUI::MapData;
using RuntimeMarker = winrt::TangramWinUI::Marker;

using winrt::Microsoft::UI::Dispatching::DispatcherQueueController;
using winrt::Microsoft::UI::Dispatching::DispatcherQueueHandler;
using winrt::Windows::Foundation::IAsyncAction;
using winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority;
using winrt::Microsoft::UI::Dispatching::DispatcherQueue;


namespace winrt::TangramWinUI::implementation {
struct MapController : public MapControllerT<MapController> {

    MapController();
    ~MapController() override;

    event_token OnSceneLoaded(EventHandler<uint32_t> const& handler);
    void OnSceneLoaded(event_token const& token) noexcept;

    event_token OnRegionWillChange(EventHandler<bool> const& handler);
    void OnRegionWillChange(event_token const& token) noexcept;

    event_token OnRegionDidChange(EventHandler<bool> const& handler);
    void OnRegionDidChange(event_token const& token) noexcept;

    event_token OnRegionIsChanging(EventHandler<int> const& handler);
    void OnRegionIsChanging(event_token const& token) noexcept;

    event_token OnViewComplete(EventHandler<int> const& handler);
    void OnViewComplete(event_token const& token) noexcept;

    void Init(SwapChainPanel panel);
    void RequestRender(bool forced = false);
    void HandlePanGesture(float startX, float startY, float endX, float endY);
    void HandlePinchGesture(float x, float y, float scale, float velocity);
    void HandleShoveGesture(float distance);
    void HandleFlingGesture(float x, float y, float velocityX, float velocityY);
    void HandleRotateGesture(float x, float y, float angle);
    void HandleDoubleTapGesture(float x, float y);
    void HandleTapGesture(float x, float y);
    void CaptureFrame(EventHandler<SoftwareBitmap> callback);
    IAsyncAction Shutdown();
    void SetPickRadius(float radius);
    void PickMarker(float posX, float posY, int identifier);
    void PickFeature(float posX, float posY, int identifier);
    void PickLabel(float posX, float posY, int identifier);

    CameraPosition GetCameraPosition();
    void UpdateCameraPosition(LngLat lngLat, float duration);
    void UpdateCameraPosition(LngLat sw, LngLat ne, int paddingLeft, int paddingTop, int paddingRight,
                              int paddingBottom, float duration);

    LngLat ScreenPositionToLngLat(double x, double y);
    Point LngLatToScreenPosition(double lng, double lat, bool clipToViewPort);
    void SetLayer(const hstring& layerName, const hstring& style);
    void SetTileSourceUrl(const hstring& sourceName, const hstring& url);
    hstring GetTileSourceUrl(const hstring& sourceName);
    int LoadSceneYaml(const hstring& yaml, const hstring& resourceRoot);
    bool LayerExists(const hstring& layerName);
    RuntimeMapData AddDataLayer(const hstring& layerName);
    void RemoveDataLayer(const RuntimeMapData& layer);
    void RemoveMarker(const RuntimeMarker& marker);
    RuntimeMarker AddMarker();
    void RemoveAllMarkers();

    void SetMaxZoomLevel(float maxZoomLevel);
    float GetPixelsPerMeter();
    float GetRotationAngle();

    void SetMarkerPickHandler(EventHandler<PickResult> const& handler);
    void SetFeaturePickHandler(EventHandler<PickResult> const& handler);
    void SetLabelPickHandler(EventHandler<PickResult> const& handler);
    void SetMapRegionStateIdle();
    volatile bool IsShuttingDown() const { return m_shuttingDown; }

    std::mutex& MapMutex() { return m_mapMutex;  }

    /* Methods only for internal use */

public:
    Tangram::Map& GetMap() const { return *m_map; }
    void RaiseViewCompleteEvent();
    void SetMapRegionState(MapRegionChangeState state);
    
    void MarkerSetVisible(MarkerID, bool visible);
    void MarkerSetDrawOrder(MarkerID, int drawOrder);
    void MarkerSetPointEased(MarkerID, Tangram::LngLat, float duration, Tangram::EaseType);
    void MarkerSetPoint(MarkerID, Tangram::LngLat);
    void MarkerSetBitmap(MarkerID, int width, int height, uint32_t* data);
    void MarkerSetStylingFromString(MarkerID, const char* styling);

    void LayerGenerateTiles(NativeMapData& data);

    template <typename Action> void ScheduleOnRenderThread(Action action) {
        struct AtomicDecrementer {
            std::atomic<int>& counter;
            
            ~AtomicDecrementer() {
                counter.fetch_sub(1);
            }
        };

        if (IsShuttingDown()) return;

        if (m_renderDispatcherQueueController.DispatcherQueue().TryEnqueue(
                DispatcherQueuePriority::High,
                DispatcherQueueHandler([this, action = std::move(action)] {
                    AtomicDecrementer decrementer{m_queuedRenderRequests};
                    if (IsShuttingDown()) return;
                    std::scoped_lock renderLock(m_renderMutex);
                    if (IsShuttingDown()) return;
                    action();
                }))) {

            m_queuedRenderRequests.fetch_add(1);
        }
    }

    template <typename Action> void ScheduleOnFileThread(Action action) {
        if (IsShuttingDown()) return;

        m_fileDispatcherQueueController.DispatcherQueue().TryEnqueue(
            DispatcherQueuePriority::High, DispatcherQueueHandler([this, action = std::move(action)] {
                if (IsShuttingDown()) return;
                action();
            }));
    }

    template <typename Action> void ScheduleOnWorkThread(Action action) {
        if (IsShuttingDown()) return;

        auto queue = m_workDispatcherQueueController.DispatcherQueue();

        if (queue.HasThreadAccess()) {
            std::scoped_lock workLock(m_workMutex);
            if (IsShuttingDown()) return;
            action();
            return;
        }

        m_workDispatcherQueueController.DispatcherQueue().TryEnqueue(
            DispatcherQueuePriority::Normal,
            DispatcherQueueHandler([this, action = std::move(action)] {
                if (IsShuttingDown()) return;
                std::scoped_lock workLock(m_workMutex);
                if (IsShuttingDown()) return;
                action();
            }));
    }

private:
    event<EventHandler<int>> m_onViewComplete;
    event<EventHandler<int>> m_onRegionIsChanging;
    event<EventHandler<bool>> m_onRegionWillChange;
    event<EventHandler<bool>> m_onRegionDidChange;
    event<EventHandler<uint32_t>> m_onSceneLoaded;

    std::shared_ptr<Tangram::Map> m_map;
    std::unique_ptr<::TangramWinUI::Renderer> m_renderer;

    SwapChainPanel m_panel;
    Windows::Foundation::Collections::IMap<hstring, RuntimeMapData> m_tileSources;
    Windows::Foundation::Collections::IMap<uint32_t, RuntimeMarker> m_markers;
    EventHandler<PickResult> m_markerPickHandler;
    EventHandler<PickResult> m_featurePickHandler;
    EventHandler<PickResult> m_labelPickHandler;

    DispatcherQueueController m_fileDispatcherQueueController;
    DispatcherQueueController m_renderDispatcherQueueController;
    DispatcherQueueController m_workDispatcherQueueController;
    MapRegionChangeState m_currentState{};
    DispatcherQueue m_uiDispatcherQueue{nullptr};
    
    volatile bool m_shuttingDown{};

    /* Guards the interactions on m_map instance. */
    std::mutex m_mapMutex;
    
    // these mutexes is just really needed to ensure we can stop properly when m_destroy is being set.
    std::mutex m_workMutex;
    std::mutex m_renderMutex;

    /*
      Keeps track of the currently queued actions on the render queue.
      This stops us from queing up too much draw events.
      We really need only 2 queued requests to provivde a smooth animation as 1 item is usually always in progress,
      and the 2nd one will be executed next.
     */
    std::atomic<int> m_queuedRenderRequests{};
};
} // namespace winrt::TangramWinUI::implementation

namespace winrt::TangramWinUI::factory_implementation {
struct MapController : public MapControllerT<MapController, implementation::MapController> {};
} // namespace winrt::TangramWinUI::factory_implementation