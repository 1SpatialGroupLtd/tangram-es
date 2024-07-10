#pragma once

#include "pch.h"
#include "CameraPosition.g.h"

namespace winrt::TangramWinUI::implementation {
struct CameraPosition : public CameraPositionT<CameraPosition> {
    ~CameraPosition() override = default;

    float Tilt() const { return m_tilt; }
    void Tilt(float tilt) { m_tilt = tilt; }

    float Zoom() const { return m_zoom; }
    void Zoom(float zoom) { m_zoom = zoom; }

    float Rotation() const { return m_rotation; }
    void Rotation(float rotation) { m_rotation = rotation; }

    double Latitude() const { return m_latitude; }
    void Latitude(double latitude) { m_latitude = latitude; }

    double Longitude() const { return m_longitude; }
    void Longitude(double longitude) { m_longitude = longitude; }

private:
    float m_tilt{};
    float m_zoom{};
    float m_rotation{};
    double m_latitude{};
    double m_longitude{};
};
}

namespace winrt::TangramWinUI::factory_implementation {
struct CameraPosition : public CameraPositionT<CameraPosition, implementation::CameraPosition> {};
}