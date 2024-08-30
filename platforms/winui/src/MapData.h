#pragma once

#include "pch.h"
#include "MapData.g.h"
#include "tangram.h"
#include "TangramPlatform.h"

namespace winrt::TangramWinUI::implementation {
    struct MapController;
    struct MapData : public MapDataT<MapData> {

    MapData() = default;
    ~MapData() override = default;

    uint64_t AppendOrUpdateGeoJson(array_view<uint8_t const>& value, bool generateTiles);
    uint64_t RemoveGeoJsonById(array_view<uint64_t const> &ids, bool generateTiles);
    void IsVisible(bool isVisible);
    bool IsVisible() const;
    bool CanUpdateFeatures() const;
    void CanUpdateFeatures(bool enabled);
    void Clear();
    void SetGeoJson(const hstring& geoJson);
    void SetGeoJsonFromBytes(array_view<uint8_t const>& value);
    uint32_t Id() const { return m_id; }

public:
    /* internal usage only*/

    hstring Name() { return m_name; }
    Tangram::ClientDataSource* Source();
    void Remove();
    void GenerateTiles();

    void Init(Tangram::ClientDataSource* source, MapController* controller);

    void Invalidate() {
        m_controller = nullptr;
        m_source = nullptr;
        m_id = 0;
    }
    
private:
    void GenerateTilesLockFree();
    bool IsInvalid() const;

    hstring m_name;
    uint32_t m_id{};
    MapController* m_controller{};
    Tangram::ClientDataSource* m_source{};
};
}

namespace winrt::TangramWinUI::factory_implementation {
struct MapData : public MapDataT<MapData, implementation::MapData> {
    
};
}