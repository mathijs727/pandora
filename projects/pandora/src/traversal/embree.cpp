#include "pandora/traversal/embree.h"
#include <iostream>

static void embreeErrorFunc(void* userPtr, const RTCError errorCode, const char* str)
{
    std::string errorCodeString;
    switch (errorCode) {
    case RTC_NO_ERROR:
        errorCodeString = "RTC_NO_ERROR";
        break;

    case RTC_UNKNOWN_ERROR:
    default:
        errorCodeString = "RTC_UNKNOWN_ERROR";
        break;

    case RTC_INVALID_ARGUMENT:
        errorCodeString = "RTC_INVALID_ARGUMENT";
        break;

    case RTC_INVALID_OPERATION:
        errorCodeString = "RTC_INVALID_OPERATION";
        break;

    case RTC_OUT_OF_MEMORY:
        errorCodeString = "RTC_OUT_OF_MEMORY";
        break;

    case RTC_UNSUPPORTED_CPU:
        errorCodeString = "RTC_UNSUPPORTED_CPU";
        break;
    }

    std::cout << "Embree error: " << errorCodeString << "\n";
    std::cout << str << std::endl;
}

namespace pandora {

EmbreeAccel::EmbreeAccel(const std::vector<const Shape*>& geometry)
{
    m_device = rtcNewDevice(nullptr);
    rtcDeviceSetErrorFunction2(m_device, embreeErrorFunc, nullptr);

    RTCSceneFlags sceneFlags = RTC_SCENE_STATIC | RTC_SCENE_HIGH_QUALITY;
    RTCAlgorithmFlags algorithmFlags = RTC_INTERSECT_STREAM;
    m_scene = rtcDeviceNewScene(m_device, sceneFlags, algorithmFlags);

    for (auto shape : geometry) {
        unsigned geomID = shape->addToEmbreeScene(m_scene);
        m_embreeGeomToShapeMapping[geomID] = shape;
    }

    rtcCommit(m_scene);
}

EmbreeAccel::~EmbreeAccel()
{
    rtcDeleteScene(m_scene);
    rtcDeleteDevice(m_device);
}

bool EmbreeAccel::intersect(Ray& ray)
{
    RTCRay embreeRay = {};
    embreeRay.org[0] = ray.origin.x;
    embreeRay.org[1] = ray.origin.y;
    embreeRay.org[2] = ray.origin.z;
    embreeRay.dir[0] = ray.direction.x;
    embreeRay.dir[1] = ray.direction.y;
    embreeRay.dir[2] = ray.direction.z;
    embreeRay.tnear = 0.0f;
    embreeRay.tfar = ray.t;
    embreeRay.geomID = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect(m_scene, embreeRay);

    if (embreeRay.geomID == RTC_INVALID_GEOMETRY_ID)
        return false; // Ray missed

    ray.t = embreeRay.tfar;
    ray.uv = Vec2f(embreeRay.u, embreeRay.v);
    return true;
}
}