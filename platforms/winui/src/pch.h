#pragma once

#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Input.h>

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Input.DragDrop.h>
#include <winrt/Windows.Graphics.Imaging.h>

using winrt::Windows::Graphics::Imaging::SoftwareBitmap;
using winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel;
using winrt::Windows::Foundation::EventHandler;

inline void ThrowTangramException(const char* msg) {
    throw winrt::hresult_error(E_FAIL, winrt::to_hstring(msg));
}

inline void ReverseImageDataUpsideDown(uint32_t* data, int width, int height) {
    for (int row = 0; row < height / 2; row++) {
        for (int col = 0; col < width; col++) {
            int top_index = row * width + col;
            int bottom_index = (height - row - 1) * width + col;
            uint32_t temp = data[top_index];
            data[top_index] = data[bottom_index];
            data[bottom_index] = temp;
        }
    }
}