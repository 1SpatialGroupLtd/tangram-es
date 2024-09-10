#pragma once

#include "tangram.h"
#include <memory>
#include "urlClient.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include "Renderer.h"

namespace TangramWinUI {

class TangramPlatform final: public Tangram::Platform {
public:
    
    TangramPlatform(winrt::TangramWinUI::implementation::MapController&controller, Tangram::UrlClient::Options options, winrt::array_view<const winrt::hstring>& fontPaths);
    TangramPlatform(const TangramPlatform&) = delete;
    TangramPlatform(TangramPlatform&&) = delete;
    ~TangramPlatform() override;

    void shutdown() override;
    void requestRender() const override;
    std::vector<Tangram::FontSourceHandle> systemFontFallbacksHandle() const override;
    bool startUrlRequestImpl(const Tangram::Url& _url, const Tangram::UrlRequestHandle _request, UrlRequestId& _id) override;
    void cancelUrlRequestImpl(const UrlRequestId _id) override;

private:
    winrt::TangramWinUI::implementation::MapController& m_controller;
    std::unique_ptr<Tangram::UrlClient> m_urlClient;
    std::vector<Tangram::FontSourceHandle> m_fontSourceHandles{};
};

} // namespace Tangram