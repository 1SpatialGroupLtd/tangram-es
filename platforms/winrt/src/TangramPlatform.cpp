#include "TangramPlatform.h"
#include "gl/hardware.h"
#include "log.h"
#include <cstdarg>
#include <fstream>

/* global Tangram methods */
namespace Tangram {

	void setCurrentThreadPriority(int priority) {}
	void initGLExtensions() { Tangram::Hardware::supportsMapBuffer = true; }

	void logMsg(const char* fmt, ...) {
		static FILE* f{};

		char buffer[1000];

		va_list args;
		va_start(args, fmt);
		vsprintf_s(buffer, fmt, args);
		va_end(args);
	}

} // namespace Tangram

namespace TangramWinRT {
	TangramPlatform::TangramPlatform(SwapChainPanel^ swapChainPanel, Tangram::UrlClient::Options urlClientOptions) :
		m_urlClient(std::make_unique<Tangram::UrlClient>(urlClientOptions)),
		m_swapChainPanel(swapChainPanel)
	{
		m_renderer = std::make_unique<Renderer>();
		m_renderer->InitRendererOnUiThread(swapChainPanel);
	}

	void TangramPlatform::shutdown() {
		m_urlClient.reset();
	}
	void TangramPlatform::requestRender() const {
		m_renderer->ScheduleRender();
	}
	std::vector<Tangram::FontSourceHandle> TangramPlatform::systemFontFallbacksHandle() const { return {}; }
	bool TangramPlatform::startUrlRequestImpl(const Tangram::Url& _url, const Tangram::UrlRequestHandle _request,
		UrlRequestId& _id) {
		using namespace Tangram;

		if (_url.hasHttpScheme())
		{
			auto onURLResponse = [this, _request](UrlResponse&& response) { onUrlResponse(_request, std::move(response)); };
			_id = m_urlClient->addRequest(_url.string(), onURLResponse);
			return true;
		}

		UrlResponse resp;

		// Open file
		std::ifstream infile(_url.string(), std::ios_base::binary); // and since you want bytes rather than
		if (!infile.good())
		{
			resp.error = "file not found";
			return false;
		}

		infile.seekg(0, std::ios::end);
		size_t length = infile.tellg();
		infile.seekg(0, std::ios::beg);

		std::vector<char> buffer;

		buffer.reserve(length);
		for (int i = 0; i < length; i++)
			buffer.push_back(0);

		// Read file
		infile.read(buffer.data(), length);
		resp.content = std::move(buffer);
		onUrlResponse(_request, std::move(resp));
		return false;

	}

	void TangramPlatform::cancelUrlRequestImpl(const UrlRequestId _id) {
		if (m_urlClient) { m_urlClient->cancelRequest(_id); }
	}
} // namespace Tangram