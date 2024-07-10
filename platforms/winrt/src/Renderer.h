#pragma once

#include <windows.ui.h>
#include <windows.foundation.collections.h>
#include <windows.ui.xaml.controls.h>
#include <agile.h>
#include <EGL/egl.h>

using Windows::UI::Xaml::Controls::SwapChainPanel;
using Windows::UI::Core::CoreWindow;
using Platform::Agile;

namespace TangramWinRT {
    
    class Renderer {
public:
    Renderer() : m_window(CoreWindow::GetForCurrentThread()){}
    void ScheduleRender();
    void InitRendererOnUiThread(SwapChainPanel^ swapChainPanel);

private:
    template <typename Action>
   
    void ScheduleOnUiThread(Action action)
    {
        using Windows::UI::Core::DispatchedHandler;

        m_window.Get()->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
           ref new DispatchedHandler([this, action]() { action(); }));
    }
private:
    Agile<CoreWindow> m_window;
    EGLDisplay m_display;
    EGLSurface m_surface;
    EGLConfig m_config;
    EGLContext m_context;

};
} // namespace Tangram