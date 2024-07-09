package com.mapzen.tangram.viewholder;

import android.content.Context;
import android.opengl.GLSurfaceView;

public class IndestructibleSurfaceView extends GLSurfaceView
{
    private final IndestructibleEGLContextFactory eglContextFactory;

    public IndestructibleSurfaceView(Context context) {
        super(context);
        eglContextFactory = new IndestructibleEGLContextFactory();
        this.setEGLContextFactory(eglContextFactory);
    }

    public IndestructibleEGLContextFactory getEglContextFactory()
    {
        return eglContextFactory;
    }
}