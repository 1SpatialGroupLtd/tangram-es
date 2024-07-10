#pragma once

#include "pch.h"
#include "LngLat.g.h"

namespace winrt::TangramWinUI::implementation
{
struct LngLat : public LngLatT<LngLat>
{
    ~LngLat() override = default;
    LngLat() = default;
    LngLat(double longitude, double latitude) : m_longitude(longitude), m_latitude(latitude)
    {}

    double Longitude(){return m_longitude;}
    void Longitude(double longitude){m_longitude = longitude;}
    double Latitude(){return m_latitude;}
    void Latitude(double latitude){m_latitude = latitude;}
    
private:
    double m_longitude;
    double m_latitude;
};
}

namespace winrt::TangramWinUI::factory_implementation
{
    struct LngLat : public LngLatT<LngLat, implementation::LngLat>
    {
    };
}