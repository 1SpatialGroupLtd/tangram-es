#pragma once

#include "MapController.g.h"
#include "pch.h"
#include "tangram.h"
#include "Renderer.h"
#include <mutex>

namespace TangramWinUI {
class TangramPlatform;
} // namespace TangramWinUI

enum class MapRegionChangeState {
    IDLE,
    JUMPING,
    ANIMATING,
};

using winrt::Microsoft::UI::Xaml::RoutedEventArgs;
using WinRTMapController = winrt::TangramWinUI::MapController;
using winrt::Windows::Foundation::Point;
using WinRTMapData = winrt::TangramWinUI::MapData;
using WinRTMarker = winrt::TangramWinUI::Marker;

using winrt::Microsoft::UI::Dispatching::DispatcherQueueController;
using winrt::Microsoft::UI::Dispatching::DispatcherQueueHandler;
using winrt::Windows::Foundation::IAsyncAction;
using winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority;
using winrt::Microsoft::UI::Dispatching::DispatcherQueue;

namespace winrt::TangramWinUI::implementation {
struct MapController : public MapControllerT<MapController> {
    MapController();
    MapController(SwapChainPanel panel, array_view<const hstring>& fontPaths, hstring const &assetsPath);
    ~MapController() override;

    event_token OnLoaded(EventHandler<WinRTMapController> const& handler);
    void OnLoaded(event_token const& token) noexcept;

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

    void UseCachedGlState(bool useCache);

    void RequestRender();
    void HandlePanGesture(float startX, float startY, float endX, float endY);
    void HandlePinchGesture(float x, float y, float scale, float velocity);
    void HandleShoveGesture(float distance);
    void HandleFlingGesture(float x, float y, float velocityX, float velocityY);
    void HandleRotateGesture(float x, float y, float angle);
    void HandleDoubleTapGesture(float x, float y);
    void HandleTapGesture(float x, float y);
    void HandlePanPinchRotateFlingShove(float panStartX, float panStartY, float panEndX, float panEndY, float pinchPosX,
                                   float pinchPosY, float pinchValue, float pinchVelocity, float rotPosX,
                                        float rotPosY, float rotRadians, float flingPosX, float flingPosY,
                                      float flingVelocityX, float flingVelocityY, float shoveDistance);

    void CaptureFrame(EventHandler<SoftwareBitmap> callback);
    IAsyncAction Shutdown();
    void SetPickRadius(float radius);
    void PickMarker(float posX, float posY, int identifier);
    void PickFeature(float posX, float posY, int identifier);
    void PickLabel(float posX, float posY, int identifier);

    CameraPosition GetCameraPosition();
    void UpdateCameraPosition(const CameraPosition& cameraPosition, int duration);
    void UpdateCameraPosition(LngLat sw, LngLat ne, int paddingLeft, int paddingTop, int paddingRight,
                              int paddingBottom, int duration);

    LngLat ScreenPositionToLngLat(double x, double y);
    Point LngLatToScreenPosition(double lng, double lat, bool clipToViewPort);
    void SetLayer(const hstring& layerName, const hstring& style);
    bool SetTileSourceUrl(const hstring& sourceName, const hstring& url);
    hstring GetTileSourceUrl(const hstring& sourceName);
    bool SetTileSourceUrl(const hstring& sourceName, const hstring& url);
    bool SetTileSourceVisibility(const hstring& sourceName, bool visibility);
    bool GetTileSourceVisibility(const hstring& sourceName);
    int LoadSceneYaml(const hstring& yaml, const hstring& resourceRoot, bool loadAsync);
    bool LayerExists(const hstring& layerName);
    WinRTMapData AddDataLayer(const hstring& layerName);
    void RemoveDataLayer(const WinRTMapData& layer);
    void RemoveMarker(const WinRTMarker& marker);
    WinRTMarker AddMarker();
    void RemoveAllMarkers();
    void SetMaxZoomLevel(float maxZoomLevel);
    float GetPixelsPerMeter();
    float GetRotationAngle();
    void SetRotationAngle(float angle);
    float GetZoomVelocity();

    void SetMarkerPickHandler(EventHandler<PickResult> const& handler);
    void SetFeaturePickHandler(EventHandler<PickResult> const& handler);
    void SetLabelPickHandler(EventHandler<PickResult> const& handler);
    void SetMapRegionStateIdle();
    void CancelCameraAnimation();

public:
    volatile bool IsShuttingDown() const { return m_shuttingDown; }
    std::mutex& Mutex() { return m_mapMutex;  }
    Tangram::Map& GetMap() const { return *m_map; }
    void RaiseViewCompleteEvent();
    void SetMapRegionState(MapRegionChangeState state);

    template <typename Action> void ScheduleOnFileThread(Action action) {
        if (IsShuttingDown()) return;

        m_fileDispatcherQueueController.DispatcherQueue().TryEnqueue(
            DispatcherQueuePriority::High, DispatcherQueueHandler([this, action = std::move(action)] {
                if (IsShuttingDown()) return;
                action();
            }));
    }

    template <typename Action> void ScheduleOnUIThread(Action action) {
        if (IsShuttingDown()) return;

        m_uiDispatcherQueue.TryEnqueue(
            DispatcherQueuePriority::High, DispatcherQueueHandler([this, action = std::move(action)] {
                if (IsShuttingDown()) return;
                action();
            }));
    }

    template <typename Action> void ScheduleOnWorkThread(Action action) {
        if (IsShuttingDown()) return;

        auto queue = m_workDispatcherQueueController.DispatcherQueue();

        if (queue.HasThreadAccess()) {
            action();
            return;
        }

        queue.TryEnqueue(
            DispatcherQueuePriority::High,
            DispatcherQueueHandler([this, action = std::move(action)] {
                if (IsShuttingDown()) return;
                action();
            }));
    }

    const std::string& GetAssetPath() const { return m_assetPath; }
    const std::string& GetResourcesPath() const { return m_resourcesPath; }

private:

    void RenderThread();
    
    event<EventHandler<int>> m_onViewComplete;
    event<EventHandler<int>> m_onRegionIsChanging;
    event<EventHandler<bool>> m_onRegionWillChange;
    event<EventHandler<bool>> m_onRegionDidChange;
    event<EventHandler<uint32_t>> m_onSceneLoaded;
    event<EventHandler<WinRTMapController>> m_onLoaded;

    std::unique_ptr<Tangram::Map> m_map;
    std::unique_ptr<::TangramWinUI::Renderer> m_renderer;

    SwapChainPanel m_panel;
    Windows::Foundation::Collections::IMap<hstring, WinRTMapData> m_tileSources;
    Windows::Foundation::Collections::IMap<uint32_t, WinRTMarker> m_markers;
    EventHandler<PickResult> m_markerPickHandler;
    EventHandler<PickResult> m_featurePickHandler;
    EventHandler<PickResult> m_labelPickHandler;

    int m_newWidth{}, m_newHeight{};
    float m_newPixelScale{};

    DispatcherQueueController m_fileDispatcherQueueController;
    DispatcherQueueController m_renderDispatcherQueueController;
    DispatcherQueueController m_workDispatcherQueueController;
    MapRegionChangeState m_currentState{};
    DispatcherQueue m_uiDispatcherQueue{nullptr};
    
    volatile bool m_shuttingDown{};

    /* Guards the interactions on m_map instance. */
    std::mutex m_mapMutex;
    std::mutex m_resizeMutex;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> m_resizeRequestedAt;

    /*
      Keeps track of the currently queued actions on the render queue.
      This stops us from queing up too much draw events.
      We really need only 2 queued requests to provide a smooth animation as 1 item is usually always in progress,
      and the 2nd one will be executed next.
     */
    std::atomic<uint64_t> m_renderRequestId{};
    bool m_loaded{};

    std::string m_resourcesPath;
    std::string m_assetPath;
};
} // namespace winrt::TangramWinUI::implementation

namespace winrt::TangramWinUI::factory_implementation {
struct MapController : public MapControllerT<MapController, implementation::MapController> {};
} // namespace winrt::TangramWinUI::factory_implementation