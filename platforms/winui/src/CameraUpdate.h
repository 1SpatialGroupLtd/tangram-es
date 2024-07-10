#pragma once

#include "pch.h"
#include "CameraUpdate.g.h"

namespace winrt::TangramWinUI::implementation
{
struct CameraUpdate : public CameraUpdateT<CameraUpdate>
{
    ~CameraUpdate() override = default;
};
}

namespace winrt::TangramWinUI::factory_implementation
{
struct CameraUpdate : public CameraUpdateT<CameraUpdate, implementation::CameraUpdate>
{
};
}