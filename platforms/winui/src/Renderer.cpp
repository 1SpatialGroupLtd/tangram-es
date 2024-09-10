#include "Renderer.h"
#include "TangramPlatform.h"
#include "platform_gl.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

#include "MapController.h"

// ensures the exclusive access to the underlying graphics API 
static std::mutex s_globalRenderMutex{};

const EGLint configAttributes[] = {EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_ALPHA_SIZE,
                                   8,
                                   EGL_DEPTH_SIZE,
                                   8,
                                   EGL_STENCIL_SIZE,
                                   8,
                                   EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES3_BIT,
                                   EGL_SAMPLE_BUFFERS,
                                   1,
                                   EGL_SAMPLES,
                                   4,
                                   EGL_NONE};

const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

const EGLint defaultDisplayAttributes[] = {
    // These are the default display attributes, used to request ANGLE's D3D11 renderer.
    // eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
    EGL_DONT_CARE,
    EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
    EGL_DONT_CARE,
    EGL_NONE,
};

const EGLint surfaceAttributes[] = {
    EGL_NONE,
};

namespace TangramWinUI {
void Renderer::InitRendererOnUiThread(SwapChainPanel& swapChainPanel) {

    std::scoped_lock globalLock(s_globalRenderMutex);

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));

    if (!eglGetPlatformDisplayEXT) { ThrowTangramException("Failed to get function eglGetPlatformDisplayEXT"); }

    // it is a very hack, unstable thing that might broke with newer version of ANGLE,
    // but it lets use create new contexts...
    static int displayId{0};
    m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void*>(displayId), defaultDisplayAttributes);
    ++displayId;

    if (m_display == EGL_NO_DISPLAY) { ThrowTangramException("Failed to get EGL display"); }

    if (eglInitialize(m_display, nullptr, nullptr) == EGL_FALSE) { ThrowTangramException("eglInitialize failed"); }

    EGLint numConfigs = 0;

    if (eglChooseConfig(m_display, configAttributes, &m_config, 1, &numConfigs) == EGL_FALSE || numConfigs == 0) {
        ThrowTangramException("Failed to choose first EGLConfig");
    }

    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttributes);

    const auto inspectable = swapChainPanel.as<IInspectable>().get();

    if (m_context == EGL_NO_CONTEXT) { ThrowTangramException("Failed to create EGL context"); }

    m_surface = eglCreateWindowSurface(m_display, m_config, inspectable, surfaceAttributes);

    if (m_surface == EGL_NO_SURFACE) { ThrowTangramException("Failed to create EGL surface"); }
}

void Renderer::MakeActive() {
    std::scoped_lock globalLock(s_globalRenderMutex);
    eglMakeCurrent(m_display, m_surface, m_surface, m_context);
}

void Renderer::Destroy() {
    std::scoped_lock globalLock(s_globalRenderMutex);
    if (!m_context) return;

    eglDestroyContext(m_display, m_context);
    eglDestroySurface(m_display, m_surface);
    
    m_context = {};
    m_surface = {};

    m_captureFrameCallback = nullptr;
    m_controller = nullptr;
}

void Renderer::CaptureFrame(CaptureCallback callback) {
    m_captureFrameCallback = std::move(callback);
    m_controller->RequestRender();
}

void Renderer::Render() {
    auto& map = m_controller->GetMap();
    Tangram::MapState state;
    bool willCaptureFrame;

    // just a hint for the workers to prepare for oncoming swap
    eglPrepareSwapBuffersANGLE(m_display, m_surface);

    {
        {
            std::scoped_lock lock(m_controller->Mutex());
            auto now = std::chrono::high_resolution_clock::now();
            m_lastTime = now;
            float elapsed_seconds = std::chrono::duration<float>(now - m_lastTime).count();
            state = map.update(elapsed_seconds);
            willCaptureFrame = m_captureFrameCallback && state.viewComplete();

            // only one thread can access the graphics layer exclusively, limitation we need to live with
            std::scoped_lock globalLock(s_globalRenderMutex);
            map.render();
        }

        // if we are not capturing, just swap the buffers right now under this lock
        if (!willCaptureFrame)
            eglSwapBuffers(m_display, m_surface);
    }

    const bool viewComplete = state.viewComplete();
    const bool isCameraEasing = state.viewChanging();
    const bool isAnimating = state.isAnimating();
    const bool mapViewBecameCompleted = viewComplete && !m_isPrevMapViewComplete;

    if (state.isAnimating()) {
        m_controller->RequestRender();
    }
    
    if (isCameraEasing) {
        if (!m_isPrevCameraEasing) {
             m_controller->SetMapRegionState(MapRegionChangeState::ANIMATING);
        }
    } else if (m_isPrevCameraEasing) {
        m_controller->SetMapRegionState(MapRegionChangeState::IDLE);
    }

    if (isAnimating) { m_controller->RequestRender(); }
    
    if (mapViewBecameCompleted) {
        m_controller->RaiseViewCompleteEvent();
    }
    
    if (willCaptureFrame) {
        using winrt::Windows::Foundation::MemoryBuffer;
        using winrt::Windows::Storage::Streams::Buffer;
        auto width = map.getViewportWidth();
        auto height = map.getViewportHeight();

        // create sufficient size buffer
        auto buffer = Buffer(width * height * sizeof(uint32_t));
        buffer.Length(buffer.Capacity());

        map.captureSnapshot(reinterpret_cast<unsigned*>(buffer.data()));

        {
            std::scoped_lock lock(m_controller->Mutex());
            // only one thread can access the graphics layer exclusively, limitation we need to live with
            std::scoped_lock globalLock(s_globalRenderMutex);
        }

        // now swap the buffer as we captured the buffer
        eglSwapBuffers(m_display, m_surface);

        m_controller->ScheduleOnWorkThread([width, height, buffer, callback = std::move(m_captureFrameCallback)] {
            // the captured data is up-side down as it emulates openg that is being upside down,
            // but d3d is upside, so lets fix it
            ReverseImageDataUpsideDown((uint32_t*)buffer.data(), width, height);

            auto bitmap = SoftwareBitmap(winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8, width, height);
            bitmap.CopyFromBuffer(buffer);

            callback(bitmap);
        });
    }

    m_isPrevCameraEasing = isCameraEasing;
    m_isPrevMapViewComplete = viewComplete;
}

} // namespace TangramWinUI
