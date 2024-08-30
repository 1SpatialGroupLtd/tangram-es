#include "MapData.h"
#include "MapData.g.cpp"

#include "MapController.h"

namespace winrt::TangramWinUI::implementation {
uint64_t MapData::AppendOrUpdateGeoJson(array_view<uint8_t const>& value, bool generateTiles) {
    if (value.size() == 0) return 0;

    if (IsInvalid()) return 0;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return 0;
    
    auto result = m_source->appendOrUpdateFeatures(reinterpret_cast<const char*>(value.begin()), value.size());
    if (generateTiles) GenerateTilesLockFree();
    return result;
}

uint64_t MapData::RemoveGeoJsonById(array_view<uint64_t const>& ids, bool generateTiles) {
    if (ids.size() == 0) return 0;

    if (IsInvalid()) return 0;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return 0;

    auto result = m_source->removeFeatures(ids.begin(), ids.size());
    if (generateTiles) GenerateTilesLockFree();
    return result;
}

void MapData::IsVisible(bool isVisible) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;

    m_source->setVisible(isVisible);
}

bool MapData::IsVisible() const {
    if (IsInvalid()) return false;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return false;

    return m_source->isVisible();
}

bool MapData::CanUpdateFeatures() const {
    if (IsInvalid()) return false;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return false;

    return m_source->canUpdateFeatures();
}

void MapData::CanUpdateFeatures(bool enabled) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;

    m_source->setCanUpdateFeatures(enabled);
}

void MapData::Clear() {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;

    m_source->clearFeatures();
    GenerateTilesLockFree();
}

void MapData::GenerateTiles() {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;

    GenerateTilesLockFree();
}

void MapData::Init(Tangram::ClientDataSource* source, MapController* controller) {
    m_source = source;
    m_controller = controller;
    m_name = to_hstring(source->name());
    m_id = source->id();
}

void MapData::GenerateTilesLockFree() {
    auto &map = m_controller->GetMap();
    map.clearTileCache(m_id);
    m_source->generateTiles();
    m_controller->RequestRender();
}

bool MapData::IsInvalid() const {
    return !m_controller || m_controller->IsShuttingDown();
}

void MapData::SetGeoJson(const hstring& geoJson) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;

    m_source->clearFeatures();
    m_source->addData(reinterpret_cast<const char*>(geoJson.begin()), geoJson.size());

    GenerateTilesLockFree();
}

void MapData::SetGeoJsonFromBytes(array_view<uint8_t const>& value) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;
    
    m_source->clearFeatures();
    m_source->addData(reinterpret_cast<const char*>(value.begin()), value.size());

    GenerateTilesLockFree();
}

void MapData::Remove() {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if (IsInvalid()) return;
    
    m_controller->RemoveDataLayer(*this);
    Invalidate();
}

Tangram::ClientDataSource* MapData::Source() { return m_source; }

} // namespace winrt::TangramWinUI::implementation
