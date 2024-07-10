#include "MapData.h"
#include "MapData.g.cpp"

#include "MapController.h"

namespace winrt::TangramWinUI::implementation {
uint64_t MapData::AppendOrUpdateGeoJson(array_view<uint8_t const>& value, bool generateTiles) {
    if (value.size() == 0 || IsInvalid()) return 0;
    auto result = m_source->appendOrUpdateFeatures(std::string(reinterpret_cast<const char*>(value.begin()), value.size()));
    if (generateTiles) GenerateTiles();
    return result;
}

uint64_t MapData::RemoveGeoJsonById(array_view<uint64_t const>& ids, bool generateTiles) {
    if (ids.size() == 0 || IsInvalid()) return 0;

    auto result = m_source->removeFeatures(ids.begin(), ids.size());
    if (generateTiles) GenerateTiles();
    return result;
}

void MapData::IsVisible(bool isVisible) {
    if (IsInvalid()) return;
    m_source->setVisible(isVisible);
}

bool MapData::IsVisible() const {
    if (IsInvalid()) return false;
    return m_source->isVisible();
}

bool MapData::CanUpdateFeatures() const {
    if (IsInvalid()) return false;
    return m_source->canUpdateFeatures();
}

void MapData::CanUpdateFeatures(bool enabled) {
    if (IsInvalid()) return;
    m_source->setCanUpdateFeatures(enabled);
}

void MapData::Clear() {
    if (IsInvalid()) return;
    m_source->clearFeatures();
    GenerateTiles();
}

void MapData::GenerateTiles() {
    if (IsInvalid()) return;
    m_controller->LayerGenerateTiles(*this);
}

void MapData::SetGeoJson(const hstring& geoJson) {
    if (IsInvalid()) return;
    m_source->clearFeatures();
    m_source->addData(to_string(geoJson));
    GenerateTiles();
}

void MapData::SetGeoJsonFromBytes(array_view<uint8_t const>& value) {
    if (IsInvalid()) return;
    m_source->clearFeatures();
    m_source->addData(std::string(reinterpret_cast<const char*>(value.begin()), value.size()));
    GenerateTiles();
}

uint32_t MapData::Id() const { return m_id; }

void MapData::Id(uint32_t id) { m_id = id; }

Tangram::ClientDataSource* MapData::Source() { return m_source; }

void MapData::SetController(MapController* controller) {
    m_controller = controller;
}

void MapData::SetSource(Tangram::ClientDataSource* source) { m_source = std::move(source); }

} // namespace winrt::TangramWinUI::implementation
