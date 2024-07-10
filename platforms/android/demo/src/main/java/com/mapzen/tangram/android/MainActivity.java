package com.mapzen.tangram.android;

import android.os.Bundle;
import android.view.Window;
import android.widget.RelativeLayout;

import androidx.appcompat.app.AppCompatActivity;

import com.mapzen.tangram.CameraUpdateFactory;
import com.mapzen.tangram.LngLat;
import com.mapzen.tangram.MapController;
import com.mapzen.tangram.MapData;
import com.mapzen.tangram.MapView;
import com.mapzen.tangram.SceneError;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

public class MainActivity extends AppCompatActivity implements MapView.MapReadyCallback, MapController.SceneLoadListener {
    MapController map;
    MapView view;
    RelativeLayout parent;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.main);
        parent = findViewById(R.id.parent);
        view = new com.mapzen.tangram.MapView(this);

        parent.addView(view);

        findViewById(R.id.button).setOnClickListener(v -> {
        });

        view.getMapAsync(this);
    }

    @Override
    public void onMapReady(MapController mapController) {
        view.setCanDestroyGlContext(false);
        map = mapController;

        map.setSceneLoadListener(this);
        map.loadSceneFileAsync("scene.yaml");
    }

    public void onSceneReady(int sceneId, SceneError sceneError) {
        map.updateCameraPosition(CameraUpdateFactory.newLngLatZoom(new LngLat(-3.1944921855771486, 47.71273792666892), 12));
    }

    String readFile(String fileName) {
        BufferedReader reader = null;
        try {
            try {
                reader = new BufferedReader(new InputStreamReader(getAssets().open(fileName)));
            } catch (IOException e) {
                throw new RuntimeException(e);
            }

            StringBuilder builder = new StringBuilder();
            // do reading, usually loop until end of file reading
            String mLine;
            while ((mLine = reader.readLine()) != null) {
                builder.append(mLine);
                builder.append("\n");
            }

            return builder.toString();
        } catch (IOException e) {
            //log the exception
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    //log the exception
                }
            }
        }

        return null;
    }
}