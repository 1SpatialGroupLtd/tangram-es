#include "Renderer.h"
#include "platform_gl.h"

#include <angle_windowsstore.h>
#include <vccorlib.h>
#include <windows.system.h>

using namespace Platform;

const EGLint configAttributes[] =
{
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_DEPTH_SIZE, 8,
	EGL_STENCIL_SIZE, 8,
	EGL_NONE
};

const EGLint contextAttributes[] =
{
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

const EGLint defaultDisplayAttributes[] =
{
	// These are the default display attributes, used to request ANGLE's D3D11 renderer.
	// eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
	EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

	// EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call 
	// the IDXGIDevice3::Trim method on behalf of the application when it gets suspended. 
	// Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
	EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
	EGL_NONE,
};

const EGLint fl9_3DisplayAttributes[] =
{
	// These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
	// These attributes are used if the call to eglInitialize fails with the default display attributes.
	EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
	EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
	EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
	EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
	EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
	EGL_NONE,
};

const EGLint warpDisplayAttributes[] =
{
	// These attributes can be used to request D3D11 WARP.
	// They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
	EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
	EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
	EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
	EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
	EGL_NONE,
};
namespace TangramWinRT {
	void Renderer::InitRendererOnUiThread(SwapChainPanel^ swapChainPanel) {

		PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
			reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
		if (!eglGetPlatformDisplayEXT) {
			throw Exception::CreateException(E_FAIL, L"Failed to get function eglGetPlatformDisplayEXT");
		}

		m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
		if (m_display == EGL_NO_DISPLAY) { throw Exception::CreateException(E_FAIL, L"Failed to get EGL display"); }

		if (eglInitialize(m_display, NULL, NULL) == EGL_FALSE) {
			throw Exception::CreateException(E_FAIL, L"eglInitialize");
		}

		EGLint numConfigs = 0;

		if ((eglChooseConfig(m_display, configAttributes, &m_config, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0)) {
			throw Exception::CreateException(E_FAIL, L"Failed to choose first EGLConfig");
		}

		m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttributes);

		if (m_context == EGL_NO_CONTEXT) {
			throw Exception::CreateException(E_FAIL, L"Failed to create EGL context");
		}

		const EGLint surfaceAttributes[] =
		{
			// EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (see above).
			// If you have compilation issues with it then please update your Visual Studio templates.
			EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
			EGL_NONE
		};

		// Create a PropertySet and initialize with the EGLNativeWindowType.
		auto surfaceCreationProperties = ref new Windows::Foundation::Collections::PropertySet();
		surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), swapChainPanel);

		m_surface = eglCreateWindowSurface(m_display, m_config, reinterpret_cast<IInspectable*>(surfaceCreationProperties), surfaceAttributes);

		if (m_surface == EGL_NO_SURFACE)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to create EGL surface");
		}

		if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to make EGLSurface current");
		}
	}

	void Renderer::ScheduleRender() {
		using Windows::UI::Core::IdleDispatchedHandler;
		using Windows::UI::Core::IdleDispatchedHandlerArgs;

		ScheduleOnUiThread([this]() {
			glFlush();
		eglSwapBuffers(m_display, m_surface); });
	}

} // namespace Tangram