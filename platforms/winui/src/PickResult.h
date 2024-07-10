#pragma once

#include "pch.h"
#include "PickResult.g.h"

namespace winrt::TangramWinUI::implementation {
struct PickResult : public PickResultT<PickResult> {
    ~PickResult() override = default;
    
    void MarkerId(uint32_t id) { m_markerId = id;}
    uint32_t MarkerId() { return m_markerId; }
    uint32_t FeatureId() const { return m_featureId; }
    void FeatureId(uint32_t id) { m_featureId = id; }
    LngLat Position() const { return m_position; }
    void Position(LngLat position) { m_position = position; }
    uint32_t Identifier() const { return m_identifier; }
    void Identifier(int id) { m_identifier = id; }
    int LabelType() const { return m_labelType; }
    void LabelType(int type) { m_labelType = type; }
    
private:
    uint32_t m_markerId{};
    uint32_t m_labelType{};
    LngLat m_position;
    uint32_t m_featureId{};
    int m_identifier{};
};
}

namespace winrt::TangramWinUI::factory_implementation {
struct PickResult : public PickResultT<PickResult, implementation::PickResult> {};
}