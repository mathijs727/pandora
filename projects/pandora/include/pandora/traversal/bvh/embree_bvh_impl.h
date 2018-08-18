namespace pandora {

template <typename LeafObj>
tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<SurfaceInteraction>>> EmbreeBVH<LeafObj>::s_intersectionDataSI;
template <typename LeafObj>
tbb::enumerable_thread_specific<std::pair<gsl::span<Ray>, gsl::span<RayHit>>> EmbreeBVH<LeafObj>::s_intersectionDataRayHit;
template <typename LeafObj>
tbb::enumerable_thread_specific<bool> EmbreeBVH<LeafObj>::s_computeSurfaceInteraction;

template <typename LeafObj>
inline EmbreeBVH<LeafObj>::EmbreeBVH(EmbreeBVH&& other)
    : m_device(std::move(other.m_device))
    , m_scene(std::move(other.m_scene))
//m_intersectRayData(other.m_intersectRayData)
{
    other.m_device = nullptr;
    other.m_scene = nullptr;
}

template <typename LeafObj>
EmbreeBVH<LeafObj>::~EmbreeBVH()
{
    if (m_scene)
        rtcReleaseScene(m_scene);
    if (m_device)
        rtcReleaseDevice(m_device);
}

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

template <typename LeafObj>
EmbreeBVH<LeafObj>::EmbreeBVH()
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);

    m_scene = rtcNewScene(m_device);
    rtcSetSceneBuildQuality(m_scene, RTC_BUILD_QUALITY_HIGH);
}

template <typename LeafObj>
inline void EmbreeBVH<LeafObj>::build(gsl::span<const LeafObj*> objects)
{
    for (const auto* objectPtr : objects) {
        RTCGeometry geom = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_USER);
        rtcAttachGeometry(m_scene, geom); // Returns geomID
        rtcSetGeometryUserPrimitiveCount(geom, objectPtr->numPrimitives());
        rtcSetGeometryUserData(geom, (void*)objectPtr);
        rtcSetGeometryBoundsFunction(geom, geometryBoundsFunc, nullptr);
        rtcSetGeometryIntersectFunction(geom, geometryIntersectFunc);
        rtcCommitGeometry(geom);
        rtcReleaseGeometry(geom);
    }
    rtcCommitScene(m_scene);
}

template <typename LeafObj>
inline bool EmbreeBVH<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    auto rays = gsl::make_span(&ray, 1);
    auto surfaceInteractions = gsl::make_span(&si, 1);

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    convertRays<1>(rays, reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    s_computeSurfaceInteraction.local() = true;
    s_intersectionDataSI.local() = { rays, surfaceInteractions };
    rtcIntersect1(m_scene, &context, &embreeRayHit);

    return embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID;
}

template <typename LeafObj>
inline bool EmbreeBVH<LeafObj>::intersect(Ray& ray, RayHit& hitInfo) const
{
    auto rays = gsl::make_span(&ray, 1);
    auto hitInfos = gsl::make_span(&hitInfo, 1);

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    convertRays<1>(rays, reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    s_computeSurfaceInteraction.local() = false;
    s_intersectionDataRayHit.local() = { rays, hitInfos };
    rtcIntersect1(m_scene, &context, &embreeRayHit);

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        return true;
    } else {
        return false;
    }
}

template <typename LeafObj>
void EmbreeBVH<LeafObj>::geometryBoundsFunc(const RTCBoundsFunctionArguments* args)
{
    const LeafObj& leafNode = *reinterpret_cast<const LeafObj*>(args->geometryUserPtr);
    Bounds bounds = leafNode.getPrimitiveBounds(args->primID);

    RTCBounds* outBounds = args->bounds_o;
    outBounds->lower_x = bounds.min.x;
    outBounds->lower_y = bounds.min.y;
    outBounds->lower_z = bounds.min.z;
    outBounds->upper_x = bounds.max.x;
    outBounds->upper_y = bounds.max.y;
    outBounds->upper_z = bounds.max.z;
}

template <typename LeafObj>
void EmbreeBVH<LeafObj>::geometryIntersectFunc(const RTCIntersectFunctionNArguments* args)
{
    const LeafObj& leafNode = *reinterpret_cast<const LeafObj*>(args->geometryUserPtr);

    int* valid = args->valid;
    RTCRayHit* rayHit = reinterpret_cast<RTCRayHit*>(args->rayhit);
    RTCRay& embreeRay = rayHit->ray;
    RTCHit& embreeHit = rayHit->hit;

    assert(args->N == 1);
    if (!valid[0])
        return;

    if (s_computeSurfaceInteraction.local()) {
        if constexpr (is_bvh_Leaf_obj<LeafObj>::has_intersect_si) {
            const auto [rays, surfaceInteractions] = s_intersectionDataSI.local();
            Ray& ray = rays[embreeRay.id];
            ray.tnear = embreeRay.tnear;
            ray.tfar = embreeRay.tfar;

            SurfaceInteraction& si = surfaceInteractions[embreeRay.id];
            if (leafNode.intersectPrimitive(ray, si, args->primID)) {
                RTCHit potentialHit = {};
                potentialHit.instID[0] = args->context->instID[0]; // Don't care for now (may need this in the future to support instancing?)
                potentialHit.geomID = 1; // Indicate that we hit something
                embreeRay.tfar = ray.tfar;
                embreeRay.tnear = ray.tnear;
                embreeHit = potentialHit;
            }
        }
    } else {
        if constexpr (is_bvh_Leaf_obj<LeafObj>::has_intersect_ray_hit) {
            const auto[rays, hitInfos] = s_intersectionDataRayHit.local();
            Ray& ray = rays[embreeRay.id];
            ray.tnear = embreeRay.tnear;
            ray.tfar = embreeRay.tfar;

            RayHit& hitInfo = hitInfos[embreeRay.id];
            if (leafNode.intersectPrimitive(ray, hitInfo, args->primID)) {
                RTCHit potentialHit = {};
                potentialHit.instID[0] = args->context->instID[0]; // Don't care for now (may need this in the future to support instancing?)
                potentialHit.geomID = 1; // Indicate that we hit something
                embreeRay.tfar = ray.tfar;
                embreeRay.tnear = ray.tnear;
                embreeHit = potentialHit;
            }
        }
    }
}

}
