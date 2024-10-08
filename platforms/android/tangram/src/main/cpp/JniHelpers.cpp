#include "JniHelpers.h"
#include "map.h"
#include <codecvt>
#include <locale>

namespace Tangram {

JavaVM* JniHelpers::s_jvm = nullptr;

static jfieldID cameraPositionLongitudeFID = nullptr;
static jfieldID cameraPositionLatitudeFID = nullptr;
static jfieldID cameraPositionZoomFID = nullptr;
static jfieldID cameraPositionRotationFID = nullptr;
static jfieldID cameraPositionTiltFID = nullptr;

static jfieldID lngLatLongitudeFID = nullptr;
static jfieldID lngLatLatitudeFID = nullptr;

static jfieldID pointFxFID = nullptr;
static jfieldID pointFyFID = nullptr;

static jfieldID edgePaddingLeftFID = nullptr;
static jfieldID edgePaddingTopFID = nullptr;
static jfieldID edgePaddingRightFID = nullptr;
static jfieldID edgePaddingBottomFID = nullptr;

void JniHelpers::jniOnLoad(JavaVM* jvm, JNIEnv* jniEnv) {
    s_jvm = jvm;

    jclass cameraPositionClass = jniEnv->FindClass("com/mapzen/tangram/CameraPosition");
    cameraPositionLongitudeFID = jniEnv->GetFieldID(cameraPositionClass, "longitude", "D");
    cameraPositionLatitudeFID = jniEnv->GetFieldID(cameraPositionClass, "latitude", "D");
    cameraPositionZoomFID = jniEnv->GetFieldID(cameraPositionClass, "zoom", "F");
    cameraPositionRotationFID = jniEnv->GetFieldID(cameraPositionClass, "rotation", "F");
    cameraPositionTiltFID = jniEnv->GetFieldID(cameraPositionClass, "tilt", "F");

    jclass lngLatClass = jniEnv->FindClass("com/mapzen/tangram/LngLat");
    lngLatLongitudeFID = jniEnv->GetFieldID(lngLatClass, "longitude", "D");
    lngLatLatitudeFID = jniEnv->GetFieldID(lngLatClass, "latitude", "D");

    jclass pointFClass = jniEnv->FindClass("android/graphics/PointF");
    pointFxFID = jniEnv->GetFieldID(pointFClass, "x", "F");
    pointFyFID = jniEnv->GetFieldID(pointFClass, "y", "F");

    jclass rectClass = jniEnv->FindClass("com/mapzen/tangram/EdgePadding");
    edgePaddingLeftFID = jniEnv->GetFieldID(rectClass, "left", "I");
    edgePaddingTopFID = jniEnv->GetFieldID(rectClass, "top", "I");
    edgePaddingRightFID = jniEnv->GetFieldID(rectClass, "right", "I");
    edgePaddingBottomFID = jniEnv->GetFieldID(rectClass, "bottom", "I");
}

std::string JniHelpers::stringFromJavaString(JNIEnv* jniEnv, jstring javaString) {
    if (javaString == nullptr) {
        return {};
    }
    auto length = jniEnv->GetStringLength(javaString);
    std::u16string chars(length, char16_t());
    if(!chars.empty()) {
        jniEnv->GetStringRegion(javaString, 0, length, reinterpret_cast<jchar*>(&chars[0]));
    }
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(chars);
}

std::string JniHelpers::stringFromJavaByteArray(JNIEnv* jniEnv, jbyteArray array) {
    if (array == nullptr)
        return {};

    jsize num_bytes = jniEnv->GetArrayLength(array);

    if(num_bytes == 0)
        return {};

    jbyte* elements = jniEnv->GetByteArrayElements(array, nullptr);
    auto str = std::string(reinterpret_cast<char*>(elements), static_cast<unsigned int>(num_bytes));

    jniEnv->ReleaseByteArrayElements(array, elements, 0);
    return std::move(str);
}

jstring JniHelpers::javaStringFromString(JNIEnv* jniEnv, const std::string& string) {
    auto chars = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(string);
    auto s = reinterpret_cast<const jchar*>(chars.empty() ? u"" : chars.data());
    return jniEnv->NewString(s, chars.length());
}

void JniHelpers::cameraPositionToJava(JNIEnv* env, jobject javaCamera, const CameraPosition& camera) {
    if (javaCamera == nullptr) {
        return;
    }
    env->SetDoubleField(javaCamera, cameraPositionLongitudeFID, camera.longitude);
    env->SetDoubleField(javaCamera, cameraPositionLatitudeFID, camera.latitude);
    env->SetFloatField(javaCamera, cameraPositionZoomFID, camera.zoom);
    env->SetFloatField(javaCamera, cameraPositionRotationFID, camera.rotation);
    env->SetFloatField(javaCamera, cameraPositionTiltFID, camera.tilt);
}

void JniHelpers::vec2ToJava(JNIEnv* env, jobject javaPointF, float x, float y) {
    if (javaPointF == nullptr) {
        return;
    }
    env->SetFloatField(javaPointF, pointFxFID, x);
    env->SetFloatField(javaPointF, pointFyFID, y);
}

void JniHelpers::lngLatToJava(JNIEnv* env, jobject javaLngLat, const LngLat& lngLat) {
    if (javaLngLat == nullptr) {
        return;
    }
    env->SetDoubleField(javaLngLat, lngLatLongitudeFID, lngLat.longitude);
    env->SetDoubleField(javaLngLat, lngLatLatitudeFID, lngLat.latitude);
}

LngLat JniHelpers::lngLatFromJava(JNIEnv* env, jobject javaLngLat) {
    if (javaLngLat == nullptr) {
        return {};
    }
    double longitude = env->GetDoubleField(javaLngLat, lngLatLongitudeFID);
    double latitude = env->GetDoubleField(javaLngLat, lngLatLatitudeFID);
    return LngLat{ longitude, latitude };
}

EdgePadding JniHelpers::edgePaddingFromJava(JNIEnv* env, jobject javaEdgePadding) {
    if (javaEdgePadding == nullptr) {
        return {};
    }
    int left = env->GetIntField(javaEdgePadding, edgePaddingLeftFID);
    int top = env->GetIntField(javaEdgePadding, edgePaddingTopFID);
    int right = env->GetIntField(javaEdgePadding, edgePaddingRightFID);
    int bottom = env->GetIntField(javaEdgePadding, edgePaddingBottomFID);
    return EdgePadding{ left, top, right, bottom };
}

void JniHelpers::edgePaddingToJava(JNIEnv* env, jobject javaEdgePadding, const EdgePadding& padding) {
    if (javaEdgePadding == nullptr) {
        return;
    }
    env->SetIntField(javaEdgePadding, edgePaddingLeftFID, padding.left);
    env->SetIntField(javaEdgePadding, edgePaddingTopFID, padding.top);
    env->SetIntField(javaEdgePadding, edgePaddingRightFID, padding.right);
    env->SetIntField(javaEdgePadding, edgePaddingBottomFID, padding.bottom);
}

} // namespace Tangram
