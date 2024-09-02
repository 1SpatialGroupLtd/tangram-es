#pragma once

#include "pch.h"
#include <EGL/egl.h>
#include <chrono>
#include <functional>
#include <mutex>

namespace winrt::TangramWinUI::implementation {
    struct MapController;
}
namespace TangramWinUI {
    class Renderer {
    using CaptureCallback = std::function<void(SoftwareBitmap)>;
public:
    Renderer(winrt::TangramWinUI::implementation::MapController& controller) : m_controller(&controller) {
    }

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
        
    void CaptureFrame(CaptureCallback callback);
    void Render();
    void InitRendererOnUiThread(SwapChainPanel& swapChainPanel);
    void MakeActive();
    void Destroy();
private:
    winrt::TangramWinUI::implementation::MapController* m_controller;    
    EGLDisplay m_display{};
    EGLSurface m_surface{};
    EGLConfig m_config{};
    EGLContext m_context{};
    std::chrono::time_point<std::chrono::steady_clock> m_lastTime{};
    CaptureCallback m_captureFrameCallback;
    bool m_isPrevMapViewComplete{};
    bool m_isPrevCameraEasing{};
 };

} // namespace Tangram