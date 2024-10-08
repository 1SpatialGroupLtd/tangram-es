#pragma once

#include "map.h"
#include <jni.h>

namespace Tangram {

class AndroidPlatform;

class AndroidMap : public Map {
public:
    AndroidMap(JNIEnv* env, jobject mapController, jobject assetManager);
    void pickFeature(float posX, float posY, int identifier = 0);
    void pickLabel(float posX, float posY, int identifier = 0);
    void pickMarker(float posX, float posY, int identifier = 0);

    AndroidPlatform& androidPlatform();

    static void jniOnLoad(JavaVM* javaVM, JNIEnv* jniEnv);
    static void jniOnUnload(JavaVM* javaVM,JNIEnv* jniEnv);

protected:
    jobject m_mapController;
};

} // namespace Tangram
