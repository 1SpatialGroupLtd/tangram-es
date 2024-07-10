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
    uint32_t Id() const;
    void Id(uint32_t id);
    hstring Name() { return m_name; }
    void Name(const hstring& name) { m_name = name; }
    Tangram::ClientDataSource* Source();
    void SetController(MapController* controller);
    void SetSource(Tangram::ClientDataSource* source);
    void GenerateTiles();
    void Invalidate() {
        m_controller = nullptr;
        m_source = nullptr;
    }
    
private:
    bool IsInvalid() const { return !m_controller; }

private:
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