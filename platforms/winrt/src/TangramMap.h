#pragma once

#include "tangram.h"
#include "TangramPlatform.h"
#include <memory>

namespace TangramWinRT{

[Windows::Foundation::Metadata::WebHostHidden]
    public ref class TangramMap sealed{
public:
    void Init(SwapChainPanel^ panel);
    void RequestRender();
    void LoadScene(Platform::String^ sceneFile);
    void Update(float delta);

    static TangramMap^ CreateMap()
    {
        return ref new TangramMap();
    }
private:

    template <typename Action>
    void ScheduleOnUiThread(Action action)
    {
        using Windows::UI::Core::DispatchedHandler;

        m_window.Get()->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
            ref new DispatchedHandler([this, action]() { action(); }));
    }

    private:
    std::shared_ptr<Tangram::Map> m_map;
    Agile<CoreWindow> m_window;
};
}
