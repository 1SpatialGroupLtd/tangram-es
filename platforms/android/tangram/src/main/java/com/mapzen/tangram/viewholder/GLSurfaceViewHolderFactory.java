package com.mapzen.tangram.viewholder;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;

import com.mapzen.tangram.BuildConfig;

public class GLSurfaceViewHolderFactory implements GLViewHolderFactory {

    /**
     * Responsible to create an instance of {@link GLSurfaceViewHolder} which holds
     * an instance of {@link GLSurfaceView} for map display.
     * @param context Application Context
     * @return {@link GLSurfaceViewHolder}
     */
    @Override
    public GLViewHolder build(Context context) {
        IndestructibleSurfaceView view = new IndestructibleSurfaceView(context);

        view.setEGLContextClientVersion(2);
        view.setPreserveEGLContextOnPause(true);
        try {
            view.setEGLConfigChooser(new ConfigChooser(8, 8, 8, 0, 16, 8));
            return new GLSurfaceViewHolder(view);
        } catch(IllegalArgumentException e) {
            // TODO: print available configs to check whether we could support them
            Log.e(BuildConfig.TAG, "EGLConfig 8-8-8-0 not supported");
        }
        try {
            view.setEGLConfigChooser(new ConfigChooser(8, 8, 8, 8, 16, 8));
            return new GLSurfaceViewHolder(view);
        } catch(IllegalArgumentException e) {
            Log.e(BuildConfig.TAG, "EGLConfig 8-8-8-8 not supported");
        }
        try {
            view.setEGLConfigChooser(new ConfigChooser(5, 6, 5, 0, 16, 8));
            return new GLSurfaceViewHolder(view);
        } catch(IllegalArgumentException e) {
            Log.e(BuildConfig.TAG, "EGLConfig 5-6-5-0 not supported");
        }
        return null;
    }
}
