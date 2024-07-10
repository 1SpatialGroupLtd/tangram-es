package com.mapzen.tangram.viewholder;

import android.opengl.EGL14;
import android.opengl.GLSurfaceView;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

public class IndestructibleEGLContextFactory implements GLSurfaceView.EGLContextFactory {
    private EGLContext savedContext = null;
    private boolean canDestroyContext = true;

    public void setCanDestroyContext(boolean canDestroyContext){
        this.canDestroyContext = canDestroyContext;
    }

    public boolean canDestroyContext(){return canDestroyContext;}

    @Override
    public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
        final int[] attrib_list = {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE };

        if(savedContext != null)
            return savedContext;

        savedContext = egl.eglCreateContext(
                display,
                eglConfig,
                EGL10.EGL_NO_CONTEXT,
                attrib_list);

        return savedContext;
    }

    @Override
    public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
        if(canDestroyContext)
        {
            egl.eglDestroyContext(display, context);
            savedContext = null;
        }
    }
}