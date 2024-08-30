#include "Marker.h"
#include "Marker.g.cpp"
#include "marker/marker.h"
#include "map.h"
#include <memory>
#include "Renderer.h"
#include "MapController.h"

namespace winrt::TangramWinUI::implementation {

bool Marker::IsVisible() const { return m_isVisible; }

void Marker::IsVisible(bool isVisible) {
    if (m_isVisible == isVisible || !m_controller) return;
    m_isVisible = isVisible;
    m_controller->MarkerSetVisible(m_id, isVisible);
}

void Marker::SetDrawOrder(int drawOrder) {
    if (!m_controller) return;
    m_controller->MarkerSetDrawOrder(m_id, drawOrder);
}

void Marker::SetPointEased(LngLat point, int duration, int easeType) {
    if (!m_controller) return;
    if (easeType < 0 || easeType > 3) 
        ThrowTangramException("Valid EaseType range is: 0 - 3");

    m_controller->MarkerSetPointEased(m_id, Tangram::LngLat{point.Longitude(), point.Latitude()}, duration,
                                      (Tangram::EaseType)easeType);
}

void Marker::SetPoint(LngLat point) {
    if (!m_controller) return;
    m_controller->MarkerSetPoint(m_id, Tangram::LngLat{point.Longitude(), point.Latitude()});
}

void Marker::SetDrawable(SoftwareBitmap bitmap) {
    if (!m_controller) return;
    if (!bitmap) {
        m_controller->MarkerSetBitmap(m_id, 0, 0, nullptr);
        return;
    }
    // make sure no one can touch it while we are copying out the data
    auto source = bitmap.LockBuffer(Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);
    auto sourceRef = source.CreateReference();

    auto size = sourceRef.Capacity();
    assert(size % sizeof(uint32_t) == 0);

    // we need to reverse the image upside down, so we create a temp buffer to do so
    auto myBuff = std::make_unique<uint8_t[]>(size);
    memcpy_s(myBuff.get(), size, sourceRef.data(), size);

    uint32_t* uintPtr = reinterpret_cast<uint32_t*>(myBuff.get());

    ReverseImageDataUpsideDown(uintPtr, bitmap.PixelWidth(), bitmap.PixelHeight());
    m_controller->MarkerSetBitmap(m_id, bitmap.PixelWidth(), bitmap.PixelHeight(), uintPtr);
}

void Marker::SetStylingFromString(const hstring& styleString) {
    if (!m_controller) return;
    const auto stdString = to_string(styleString);
    m_controller->MarkerSetStylingFromString(m_id, stdString.data());
}

void Marker::UserData(IInspectable userData) { m_userData = userData; }

Windows::Foundation::IInspectable Marker::UserData() const { return m_userData; }

uint32_t Marker::Id() const { return m_id; }

void Marker::Id(uint32_t id) { m_id = id; }

void Marker::SetController(MapController* map) { m_controller = map; }

void Marker::Invalidate() { m_controller = nullptr; }
} // namespace winrt::TangramWinUI::implementation
