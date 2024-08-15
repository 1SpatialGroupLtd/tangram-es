#pragma once

#include "view/view.h"

#include <bitset>
#include <memory>

namespace Tangram {

class InputHandler {

public:
    explicit InputHandler(View& _view);

    void handleTapGesture(float _posX, float _posY);
    void handleDoubleTapGesture(float _posX, float _posY);
    void handlePanGesture(float _startX, float _startY, float _endX, float _endY);
    void handleFlingGesture(float _posX, float _posY, float _velocityX, float _velocityY);
    void handlePinchGesture(float _posX, float _posY, float _scale, float _velocity);
    void handleRotateGesture(float _posX, float _posY, float _radians);
    void handleShoveGesture(float _distance);
    void handlePanPinchRotateFlingShove(float _panStartX, float _panStartY, float _panEndX, float _panEndY,
                              float _pinchPosX, float _pinchPosY, float _pinchValue, float _pinchVelocity,
                              float _rotPosX, float _rotPosY, float _rotRadians,
                              float _flingPosX, float _flingPosY, float _flingVelocityX, float _flingVelocityY,
                              float _shoveDistance
                              );

    /*
     * Returns true if the update results in any flinging from the inputHandler
     */
    bool update(float _dt);

    void cancelFling();

    void setView(View& _view) { m_view = _view; }
    float getZoomVelocity() const { return m_velocityZoom; }
private:

    void setVelocity(float _zoom, glm::vec2 _pan);

    View& m_view;

    // fling deltas on zoom and translation
    glm::vec2 m_velocityPan;
    float m_velocityZoom = 0.f;

};

}
