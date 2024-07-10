#pragma once

#include "tangram.h"
#include "Renderer.h"
#include "urlClient.h"
#include <memory>

namespace TangramWinRT {

class TangramPlatform final : public Tangram::Platform {

public:
    TangramPlatform(SwapChainPanel^ swapChainPanel, Tangram::UrlClient::Options options);
    ~TangramPlatform() override = default;

    /* Tangram::Platform */ 
    void shutdown() override;
    void requestRender() const override;
    std::vector<Tangram::FontSourceHandle> systemFontFallbacksHandle() const override;
    bool startUrlRequestImpl(const Tangram::Url& _url, const Tangram::UrlRequestHandle _request,
                             UrlRequestId& _id) override;
    void cancelUrlRequestImpl(const UrlRequestId _id) override;

private:
    SwapChainPanel^m_swapChainPanel;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Tangram::UrlClient> m_urlClient;
};

} // namespace Tangram