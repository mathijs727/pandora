namespace pandora {

template <typename LeafObj>
tbb::enumerable_thread_specific<gsl::span<RayHit>> EmbreeBVH<LeafObj>::s_intersectionDataRayHit;

template <typename LeafObj>
inline EmbreeBVH<LeafObj>::EmbreeBVH(EmbreeBVH&& other)
    : m_device(std::move(other.m_device))
    , m_scene(std::move(other.m_scene))
    , m_memoryUsed(other.m_memoryUsed.load())
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

template <typename LeafObj>
size_t EmbreeBVH<LeafObj>::size() const
{
    size_t sizeBytes = sizeof(decltype(*this));
    sizeBytes += s_intersectionDataRayHit.size() * sizeof(gsl::span<RayHit>);
    sizeBytes += m_memoryUsed.load();
    return sizeBytes;
}

template <unsigned N>
void raysToEmbreeRayHits(gsl::span<const Ray> rays, RTCRayHitN* embreeRayHits)
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
    }
}

template <unsigned N>
void raysToEmbreeRays(gsl::span<const Ray> rays, RTCRayN* embreeRays)
{
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
    : m_memoryUsed(0)
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);
    rtcSetDeviceMemoryMonitorFunction(m_device, deviceMemoryMonitorFunction, this);

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
        rtcSetGeometryOccludedFunction(geom, geometryOccludedFunc);
        rtcCommitGeometry(geom);
        rtcReleaseGeometry(geom);
    }
    rtcCommitScene(m_scene);
}

template <typename LeafObj>
inline bool EmbreeBVH<LeafObj>::intersect(Ray& ray, RayHit& hitInfo) const
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRayHit embreeRayHit;
    raysToEmbreeRayHits<1>(gsl::make_span(&ray, 1), reinterpret_cast<RTCRayHitN*>(&embreeRayHit));

    s_intersectionDataRayHit.local() = gsl::make_span(&hitInfo, 1);
    rtcIntersect1(m_scene, &context, &embreeRayHit);

    return hitInfo.sceneObject != nullptr;
}

template <typename LeafObj>
inline bool EmbreeBVH<LeafObj>::intersectAny(Ray& ray) const
{
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    RTCRay embreeRay;
    raysToEmbreeRays<1>(gsl::make_span(&ray, 1), reinterpret_cast<RTCRayN*>(&embreeRay));
    rtcOccluded1(m_scene, &context, &embreeRay);

    ray.tfar = embreeRay.tfar;
    return embreeRay.tfar == -std::numeric_limits<float>::infinity();
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

    assert(args->N == 1);
    if (!args->valid[0])
        return;

    RTCRayHit* embreeRayHit = reinterpret_cast<RTCRayHit*>(args->rayhit);
    RTCRay& embreeRay = embreeRayHit->ray;
    RTCHit& embreeHit = embreeRayHit->hit;

    const auto hitInfos = s_intersectionDataRayHit.local();
    Ray ray;
    ray.tnear = embreeRay.tnear;
    ray.tfar = embreeRay.tfar;
    ray.origin = glm::vec3(embreeRay.org_x, embreeRay.org_y, embreeRay.org_z);
    ray.direction = glm::vec3(embreeRay.dir_x, embreeRay.dir_y, embreeRay.dir_z);

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

template <typename LeafObj>
void EmbreeBVH<LeafObj>::geometryOccludedFunc(const RTCOccludedFunctionNArguments* args)
{
    const LeafObj& leafNode = *reinterpret_cast<const LeafObj*>(args->geometryUserPtr);

    assert(args->N == 1);
    if (!args->valid[0])
        return;

    RTCRayN* embreeRay = args->ray;

    Ray ray;
    ray.tnear = RTCRayN_tnear(embreeRay, 1, 0);
    ray.tfar = RTCRayN_tfar(embreeRay, 1, 0);
    ray.origin = glm::vec3(RTCRayN_org_x(embreeRay, 1, 0), RTCRayN_org_y(embreeRay, 1, 0), RTCRayN_org_z(embreeRay, 1, 0));
    ray.direction = glm::vec3(RTCRayN_dir_x(embreeRay, 1, 0), RTCRayN_dir_y(embreeRay, 1, 0), RTCRayN_dir_z(embreeRay, 1, 0));

    RayHit hitInfo;
    if (leafNode.intersectPrimitive(ray, hitInfo, args->primID)) {
        // https://embree.github.io/api.html#rtcsetgeometryoccludedfunction
        // On a hit the ray tfar should be set to negative infinity
        RTCRayN_tfar(embreeRay, 1, 0) = -std::numeric_limits<float>::infinity();
    }
}

template <typename LeafObj>
inline bool EmbreeBVH<LeafObj>::deviceMemoryMonitorFunction(void* userPtr, int64_t bytes, bool post)
{
    auto thisPtr = reinterpret_cast<EmbreeBVH<LeafObj>*>(userPtr);
    thisPtr->m_memoryUsed.fetch_add(bytes);
    return true;
}

}
