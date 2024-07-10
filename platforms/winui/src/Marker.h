#pragma once

#include "pch.h"
#include "Marker.g.h"

namespace winrt::TangramWinUI::implementation
{
struct MapController;

struct Marker : public MarkerT<Marker>
{
    Marker() = default;
    ~Marker() override = default;

    bool IsVisible() const;
    void IsVisible(bool isVisible);
    void SetDrawOrder(int drawOrder);
    void SetPointEased(LngLat point, int duration);
    void SetPoint(LngLat point);
    void SetDrawable(SoftwareBitmap bitmap);
    void SetStylingFromString(const hstring& styleString);
    void UserData(IInspectable userData);
    IInspectable UserData() const;
    uint32_t Id() const;
    void Id(uint32_t id);

    void SetController(MapController* controller);
    void Invalidate();

private:
    IInspectable m_userData;
    bool m_isVisible{true};
    uint32_t m_id{};
    MapController* m_controller{};
};
}

namespace winrt::TangramWinUI::factory_implementation
{
    struct Marker : public MarkerT<Marker, implementation::Marker>
    {
    };
}