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
    if (IsInvalid() || m_isVisible == isVisible) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return;
    
    auto& map = m_controller->GetMap();

    m_isVisible = isVisible;
    map.markerSetVisible(m_id, isVisible);
}

void Marker::SetDrawOrder(int drawOrder) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    auto& map = m_controller->GetMap();

    map.markerSetDrawOrder(m_id, drawOrder);
}

void Marker::SetPointEased(LngLat point, int duration, int easeType) {
    if (easeType < 0 || easeType > 3) 
        ThrowTangramException("Valid EaseType range is: 0 - 3");

    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    auto& map = m_controller->GetMap();
    
    map.markerSetPointEased(m_id, Tangram::LngLat{point.Longitude(), point.Latitude()}, duration / 1000.f,
                                      static_cast<Tangram::EaseType>(easeType));
}

void Marker::SetPoint(LngLat point) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    auto& map = m_controller->GetMap();

    map.markerSetPoint(m_id, Tangram::LngLat{point.Longitude(), point.Latitude()});
}

void Marker::SetDrawable(SoftwareBitmap bitmap) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    auto& map = m_controller->GetMap();

    if (!bitmap) {
        map.markerSetBitmap(m_id, 0, 0, nullptr);
        return;
    }
    // make sure no one can touch it while we are copying out the data
    auto source = bitmap.LockBuffer(Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);
    auto sourceRef = source.CreateReference();

    const auto size = sourceRef.Capacity();
    assert(size % sizeof(uint32_t) == 0);

    // we need to reverse the image upside down, so we create a temp buffer to do so
    auto myBuff = std::make_unique<uint8_t[]>(size);
    memcpy_s(myBuff.get(), size, sourceRef.data(), size);

    auto uintPtr = reinterpret_cast<uint32_t*>(myBuff.get());
    ReverseImageDataUpsideDown(uintPtr, bitmap.PixelWidth(), bitmap.PixelHeight());
    map.markerSetBitmap(m_id, bitmap.PixelWidth(), bitmap.PixelHeight(), uintPtr);
}

void Marker::SetStylingFromString(const hstring& styleString) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    auto& map = m_controller->GetMap();

    const auto stdString = to_string(styleString);
    map.markerSetStylingFromString(m_id, stdString.data());
}

void Marker::UserData(IInspectable userData) {
    if (IsInvalid()) return;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return ;
    m_userData = userData;
}

Windows::Foundation::IInspectable Marker::UserData() const {
    if (IsInvalid()) return nullptr;
    std::scoped_lock mapLock(m_controller->Mutex());
    if(IsInvalid()) return nullptr;
    return m_userData;
}

uint32_t Marker::Id() const { return m_id; }

void Marker::Init(uint32_t id, MapController* controller) {
    m_id = id;
    m_controller = controller;
}

void Marker::Invalidate() { m_controller = nullptr; m_userData = nullptr; }
bool Marker::IsInvalid() const { return !m_controller || m_controller->IsShuttingDown(); }
} // namespace winrt::TangramWinUI::implementation
