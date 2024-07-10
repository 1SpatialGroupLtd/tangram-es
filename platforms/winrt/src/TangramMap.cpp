#include "TangramMap.h"

namespace TangramWinRT
{
	void TangramMap::Init(SwapChainPanel^ panel)
	{
		m_map = std::make_shared<Tangram::Map>(std::make_unique<TangramPlatform>(panel, Tangram::UrlClient::Options{}));
		m_map->resize(panel->ActualWidth, panel->ActualHeight);
		m_window = CoreWindow::GetForCurrentThread();
		m_map->setupGL();
	}
	void TangramMap::RequestRender()
	{
		ScheduleOnUiThread([this] {
			m_map->render();
			m_map->getPlatform().requestRender();
			});
	}
	void TangramMap::LoadScene(Platform::String^ sceneFile)
	{
		std::wstring fooW(sceneFile->Begin());
		std::string fooA(fooW.begin(), fooW.end());
		auto options = Tangram::SceneOptions();
		options.numTileWorkers = 2;
		options.memoryTileCacheSize = 16 * (1024 * 1024);
		options.url = Tangram::Url(std::move(fooA));
		m_map->loadScene(std::move(options));
	}

	void TangramMap::Update(float delta)
	{
		m_map->update(delta);
	}
}
