#include "util/inputHandler.h"

#include "glm/gtx/rotate_vector.hpp"
#include "glm/vec2.hpp"
#include <cmath>

// Damping factor for translation; reciprocal of the decay period in seconds
#define DAMPING_PAN 4.0f

// Damping factor for zoom; reciprocal of the decay period in seconds
#define DAMPING_ZOOM 6.0f

// Minimum translation at which momentum should start (pixels per second)
#define THRESHOLD_START_PAN 350.f

// Minimum translation at which momentum should stop (pixels per second)
#define THRESHOLD_STOP_PAN 24.f

// Minimum zoom at which momentum should start (zoom levels per second)
#define THRESHOLD_START_ZOOM 1.f

// Minimum zoom at which momentum should stop (zoom levels per second)
#define THRESHOLD_STOP_ZOOM 0.3f

namespace Tangram {

InputHandler::InputHandler(View& _view) : m_view(_view) {}

bool InputHandler::update(float _dt) {

    auto velocityPanPixels = m_view.pixelsPerMeter() / m_view.pixelScale() * m_velocityPan;

    bool isFlinging = glm::length(velocityPanPixels) > THRESHOLD_STOP_PAN ||
                      std::abs(m_velocityZoom) > THRESHOLD_STOP_ZOOM;

    if (isFlinging) {

        m_velocityPan -= min(_dt * DAMPING_PAN, 1.f) * m_velocityPan;
        m_view.translate(_dt * m_velocityPan.x, _dt * m_velocityPan.y);

        m_velocityZoom -= min(_dt * DAMPING_ZOOM, 1.f) * m_velocityZoom;
        m_view.zoom(m_velocityZoom * _dt);
    }

    return isFlinging;
}

void InputHandler::handleTapGesture(float _posX, float _posY) {
    cancelFling();

    float viewCenterX = 0.5f * m_view.getWidth();
    float viewCenterY = 0.5f * m_view.getHeight();

    m_view.screenToGroundPlane(viewCenterX, viewCenterY);
    m_view.screenToGroundPlane(_posX, _posY);

    m_view.translate((_posX - viewCenterX), (_posY - viewCenterY));
}

void InputHandler::handleDoubleTapGesture(float _posX, float _posY) {
    handlePinchGesture(_posX, _posY, 2.f, 0.f);
}

void InputHandler::handlePanGesture(float _startX, float _startY, float _endX, float _endY) {
    cancelFling();

    m_view.screenToGroundPlane(_startX, _startY);
    m_view.screenToGroundPlane(_endX, _endY);

    float dx = _startX - _endX;
    float dy = _startY - _endY;

    m_view.translate(dx, dy);
}

void InputHandler::handleFlingGesture(float _posX, float _posY, float _velocityX, float _velocityY) {

    if (glm::length(glm::vec2(_velocityX, _velocityY)) / m_view.pixelScale() <= THRESHOLD_START_PAN) {
        return;
    }

    const static float epsilon = 0.0167f;

    cancelFling();

    float startX = _posX;
    float startY = _posY;
    float endX = _posX + epsilon * _velocityX;
    float endY = _posY + epsilon * _velocityY;

    m_view.screenToGroundPlane(startX, startY);
    m_view.screenToGroundPlane(endX, endY);

    float dx = (startX - endX) / epsilon;
    float dy = (startY - endY) / epsilon;

    setVelocity(0.f, glm::vec2(dx, dy));
}

void InputHandler::handlePinchGesture(float _posX, float _posY, float _scale, float _velocity) {
    cancelFling();

    // Early out for an invalid scale
    if (_scale <= 0.f) {
        // 'scale' should be a ratio between two sizes with the same sign.
        return;
    }

    float z = m_view.getZoom();
    static float invLog2 = 1 / log(2);

    m_view.zoom(log(_scale) * invLog2);

    m_view.screenToGroundPlane(_posX, _posY);
    float s = pow(2, m_view.getZoom() - z) - 1;
    m_view.translate(s * _posX, s * _posY);

    // Take the derivative of zoom as a function of scale:
    // z(s) = log2(s) + C
    // z'(s) = s' / s / log(2)
    float vz = _velocity / _scale * invLog2;
    if (std::abs(vz) >= THRESHOLD_START_ZOOM) {
        setVelocity(vz, glm::vec2(0.f));
    }
}

void
InputHandler::handlePanPinchRotateFlingShove(float _panStartX, float _panStartY, float _panEndX,
                                             float _panEndY,
                                             float _pinchPosX, float _pinchPosY,
                                             float _pinchValue, float _pinchVelocity,
                                             float _rotPosX, float _rotPosY, float _rotRadians,
                                             float _flingPosX, float _flingPosY,
                                             float _flingVelocityX, float _flingVelocityY,
                                             float _shoveDistance) {

    if(_panStartX != _panEndX || _panStartY != _panEndY)
        handlePanGesture(_panStartX, _panStartY, _panEndX, _panEndY);

    if(_flingVelocityX != 0.f || _flingVelocityY != 0.f)
        handleFlingGesture(_flingPosX, _flingPosY, _flingVelocityX, _flingVelocityY);

    if(_pinchValue != 1.f && _pinchValue != 0.f)
        handlePinchGesture(_pinchPosX, _pinchPosY, _pinchValue, _pinchVelocity);

    if(_rotRadians != 0.f)
        handleRotateGesture(_rotPosX, _rotPosY, _rotRadians);

    if(_shoveDistance != 0.f)
        handleShoveGesture(_shoveDistance);

}
void InputHandler::handleRotateGesture(float _posX, float _posY, float _radians) {
    cancelFling();

    // Get vector from center of rotation to view center
    m_view.screenToGroundPlane(_posX, _posY);
    glm::vec2 offset(_posX, _posY);

    // Rotate vector by gesture rotation and apply difference as translation
    glm::vec2 translation = offset - glm::rotate(offset, _radians);
    m_view.translate(translation.x, translation.y);
    m_view.roll(_radians);
}

void InputHandler::handleShoveGesture(float _distance) {
    cancelFling();

    float angle = -M_PI * _distance / m_view.getHeight();
    m_view.pitch(angle);
}

void InputHandler::cancelFling() {
    setVelocity(0.f, { 0.f, 0.f});
}

void InputHandler::setVelocity(float _zoom, glm::vec2 _translate) {
    // setup deltas for momentum on gesture
    m_velocityPan = _translate;
    m_velocityZoom = _zoom;
}

}
