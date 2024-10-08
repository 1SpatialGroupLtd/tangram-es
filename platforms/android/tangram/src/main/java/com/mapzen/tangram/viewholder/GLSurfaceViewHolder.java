package com.mapzen.tangram.viewholder;

import android.opengl.GLSurfaceView;
import android.view.View;

import androidx.annotation.NonNull;


public class GLSurfaceViewHolder implements GLViewHolder {
    private final IndestructibleSurfaceView glSurfaceView;

    public GLSurfaceViewHolder(final IndestructibleSurfaceView glSurfaceView) {
        this.glSurfaceView = glSurfaceView;
    }

    // GLViewHolder Methods
    // =========================

    @Override
    public boolean canDestroyGlContext() {
        return glSurfaceView.getEglContextFactory().canDestroyContext();
    }

    @Override
    public void setCanDestroyGlContext(boolean canDestroy) {
        glSurfaceView.getEglContextFactory().setCanDestroyContext(canDestroy);
    }

    @Override
    public void setRenderer(GLSurfaceView.Renderer renderer) {
        glSurfaceView.setRenderer(renderer);
    }

    @Override
    public void requestRender() {
        glSurfaceView.requestRender();
    }

    @Override
    public void setRenderMode(RenderMode renderMode) {
        switch (renderMode) {
            case RENDER_WHEN_DIRTY:
                glSurfaceView.setRenderMode(0);
                break;
            case RENDER_CONTINUOUSLY:
                glSurfaceView.setRenderMode(1);
            default:
        }
    }

    @Override
    public RenderMode getRenderMode() {
        int renderMode = glSurfaceView.getRenderMode();
        switch (renderMode) {
            case 0:
                return RenderMode.RENDER_WHEN_DIRTY;
            case 1:
            default:
                // Returns the default continuous value getRenderMode() returns an invalid int value
                return RenderMode.RENDER_CONTINUOUSLY;
        }
    }

    @Override
    public void queueEvent(@NonNull final Runnable r) {
        glSurfaceView.queueEvent(r);
    }

    @Override
    public void onPause() {
        glSurfaceView.onPause();
    }

    @Override
    public void onResume() {
        glSurfaceView.onResume();
    }

    @Override
    public void onDestroy() {}

    @Override
    @NonNull
    public View getView() {
        return this.glSurfaceView;
    }
}
