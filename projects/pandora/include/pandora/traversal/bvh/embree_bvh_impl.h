#include "pandora/geometry/bounds.h"

namespace pandora {

template <typename LeafNode>
tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<SurfaceInteraction>>> EmbreeBVH<LeafNode>::m_intersectRayData;

template <unsigned N>
void convertRays(gsl::span<const Ray> rays, RTCRayHitN* embreeRayHits)
{
    RTCRayN* embreeRays = RTCRayHitN_RayN(embreeRayHits, N);
    RTCHitN* embreeHits = RTCRayHitN_HitN(embreeRayHits, N);
    for (unsigned i = 0; i < N; i++) {
        RTCRayN_org_x(embreeRays, N, i) = rays[i].origin.x;
        RTCRayN_org_y(embreeRays, N, i) = rays[i].origin.y;
        RTCRayN_org_z(embreeRays, N, i) = rays[i].origin.z;
        RTCRayN_dir_x(embreeRays, N, i) = rays[i].direction.x;
        RTCRayN_dir_y(embreeRays, N, i) = rays[i].direction.y;
        RTCRayN_dir_z(embreeRays, N, i) = rays[i].direction.z;
        RTCRayN_tnear(embreeRays, N, i) = rays[i].tnear;
        RTCRayN_tfar(embreeRays, N, i) = rays[i].tfar;
        RTCRayN_id(embreeRays, N, i) = i;

        RTCHitN_geomID(embreeHits, N, i) = RTC_INVALID_GEOMETRY_ID;
        //RTCHitN_instID(embreeHits, N, i, 0) = RTC_INVALID_GEOMETRY_ID;
    }
}

static void embreeErrorFunc(void* userPtr, const RTCError code, const char* str)
{
    switch (code) {
    case RTC_ERROR_NONE:
        std::cout << "RTC_ERROR_NONE";
        break;
    case RTC_ERROR_UNKNOWN:
        std::cout << "RTC_ERROR_UNKNOWN";
        break;
    case RTC_ERROR_INVALID_ARGUMENT:
        std::cout << "RTC_ERROR_INVALID_ARGUMENT";
        break;
    case RTC_ERROR_INVALID_OPERATION:
        std::cout << "RTC_ERROR_INVALID_OPERATION";
        break;
    case RTC_ERROR_OUT_OF_MEMORY:
        std::cout << "RTC_ERROR_OUT_OF_MEMORY";
        break;
    case RTC_ERROR_UNSUPPORTED_CPU:
        std::cout << "RTC_ERROR_UNSUPPORTED_CPU";
        break;
    case RTC_ERROR_CANCELLED:
        std::cout << "RTC_ERROR_CANCELLED";
        break;
    }

    std::cout << ": " << str << std::endl;
}

template <typename LeafNode>
EmbreeBVH<LeafNode>::EmbreeBVH()
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);

    m_scene = rtcNewScene(m_device);
    rtcSetSceneBuildQuality(m_scene, RTC_BUILD_QUALITY_HIGH);
}

template <typename LeafNode>
EmbreeBVH<LeafNode>::~EmbreeBVH()
{
    rtcReleaseScene(m_scene);
    rtcReleaseDevice(m_device);
}

template <typename LeafNode>
inline void EmbreeBVH<LeafNode>::addObject(const LeafNode* objectPtr)
{
    //auto leafNodeHandle = m_leafAllocator.allocate<LeafNode>(std::ref(leaf));
    RTCGeometry geom = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_USER);
    rtcAttachGeometry(m_scene, geom); // Returns geomID
    rtcSetGeometryUserPrimitiveCount(geom, objectPtr->numPrimitives());
	//rtcSetGeometryUserData(geom, &leafNodeHandle.get(m_leafAllocator));
	rtcSetGeometryUserData(geom, (void*)objectPtr);
    rtcSetGeometryBoundsFunction(geom, geometryBoundsFunc, nullptr);
    rtcSetGeometryIntersectFunction(geom, geometryIntersectFunc);
    rtcCommitGeometry(geom);
    rtcReleaseGeometry(geom);
}

template <typename LeafNode>
inline void EmbreeBVH<LeafNode>::commit()
{
    rtcCommitScene(m_scene);
}

template <typename LeafNode>
inline bool EmbreeBVH<LeafNode>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    auto rays = gsl::make_span(&ray, 1);
    auto surfaceInteractions = gsl::make_span(&si, 1);

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    convertRays<1>(rays, reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    m_intersectRayData.local() = { rays, surfaceInteractions };
    rtcIntersect1(m_scene, &context, &embreeRayHit);

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        si.wo = -ray.direction;
        return true;
    } else {
        return false;
    }
}

template <typename LeafNode>
void EmbreeBVH<LeafNode>::geometryBoundsFunc(const RTCBoundsFunctionArguments* args)
{
    const LeafNode& leafNode = *reinterpret_cast<const LeafNode*>(args->geometryUserPtr);
    Bounds bounds = leafNode.getPrimitiveBounds(args->primID);

    RTCBounds* outBounds = args->bounds_o;
    outBounds->lower_x = bounds.min.x;
    outBounds->lower_y = bounds.min.y;
    outBounds->lower_z = bounds.min.z;
    outBounds->upper_x = bounds.max.x;
    outBounds->upper_y = bounds.max.y;
    outBounds->upper_z = bounds.max.z;
}

template <typename LeafNode>
void EmbreeBVH<LeafNode>::geometryIntersectFunc(const RTCIntersectFunctionNArguments* args)
{
    const LeafNode& leafNode = *reinterpret_cast<const LeafNode*>(args->geometryUserPtr);

    int* valid = args->valid;
    RTCRayHit* rayHit = reinterpret_cast<RTCRayHit*>(args->rayhit);
    RTCRay& embreeRay = rayHit->ray;
    RTCHit& embreeHit = rayHit->hit;

    const auto& [rays, surfaceInteractions] = m_intersectRayData.local();
    Ray& ray = rays[embreeRay.id];
    SurfaceInteraction& si = surfaceInteractions[embreeRay.id];

    assert(args->N == 1);

    if (!valid[0])
        return;

    ray.tnear = embreeRay.tnear;
    ray.tfar = embreeRay.tfar;
    if (leafNode.intersectPrimitive(args->primID, ray, si)) {
        RTCHit potentialHit = {};
        potentialHit.instID[0] = args->context->instID[0]; // Don't care for now (may need this in the future to support instancing?)
        potentialHit.geomID = 1; // Indicate that we hit something
        embreeRay.tfar = ray.tfar;
        embreeRay.tnear = ray.tnear;
        embreeHit = potentialHit;
    }
}

}
