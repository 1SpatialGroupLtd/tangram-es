#include "TangramPlatform.h"
#include "gl/hardware.h"
#include "log.h"
#include <cstdarg>
#include <fstream>
#include <utility>
#include "MapController.h"

/* global Tangram methods */
namespace Tangram {

void setCurrentThreadPriority(int priority) {}
void initGLExtensions() { Hardware::supportsMapBuffer = true; }

void logMsg(const char* fmt, ...) {
    char buffer[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buffer, 1024, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
}

} // namespace Tangram

namespace TangramWinUI {

TangramPlatform::TangramPlatform(winrt::TangramWinUI::implementation::MapController&controller, Tangram::UrlClient::Options urlClientOptions)
    : m_controller(controller), 
      m_urlClient(std::make_unique<Tangram::UrlClient>(urlClientOptions))
{}

TangramPlatform::~TangramPlatform() { shutdown(); }

void TangramPlatform::shutdown() {
    Platform::shutdown();
    m_urlClient.reset();
}

void TangramPlatform::requestRender() const {
    m_controller.RequestRender();
}

std::vector<Tangram::FontSourceHandle> TangramPlatform::systemFontFallbacksHandle() const { return {}; }

bool TangramPlatform::startUrlRequestImpl(const Tangram::Url& _url, const Tangram::UrlRequestHandle _request,
                                          UrlRequestId& _id) {
    using namespace Tangram;

    if (m_controller.IsShuttingDown()) return false;

    if (_url.hasHttpScheme()) {
        auto onURLResponse = [this, _request](UrlResponse&& response) { 
            if (m_controller.IsShuttingDown()) return;
            onUrlResponse(_request, std::move(response));
            };

        _id = m_urlClient->addRequest(_url.string(), std::move(onURLResponse));
        // true means the request can be cancelled
        return true;
    }

    m_controller.ScheduleOnFileThread([this, _request, localPath = _url.string()] {
        UrlResponse resp;

        // Open file
        std::ifstream infile(localPath, std::ios_base::binary); // and since you want bytes rather than

        if (!infile.good()) {
            LOG("File not found: ", localPath.c_str());
            resp.error = "File not found";
            onUrlResponse(_request, std::move(resp));
            return;
        }

        infile.seekg(0, std::ios::end);
        size_t length = infile.tellg();
        infile.seekg(0, std::ios::beg);

        std::vector<char> buffer;

        // reserve the space required for the file's size
        buffer.reserve(length);
        for (int i = 0; i < length; i++) buffer.push_back(0);

        // Read file
        infile.read(buffer.data(), length);
        resp.content = std::move(buffer);
        onUrlResponse(_request, std::move(resp));
    });

    return false;
}

void TangramPlatform::cancelUrlRequestImpl(const UrlRequestId _id) {
    if (m_urlClient) { m_urlClient->cancelRequest(_id); }
}
} // namespace TangramWinUI