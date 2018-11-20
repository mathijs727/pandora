#include "pandora/integrators/sampler_integrator.h"
#include "pandora/config.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include "pandora/core/stats.h"
#include "pandora/utility/memory_arena.h"
#include "utility/fix_visitor.h"
#include <gsl/gsl>
#include <morton.h>
#include <random>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

SamplerIntegrator::SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp)
    : Integrator(scene, sensor, spp)
    , m_maxDepth(maxDepth)
    , m_cameraThisFrame(nullptr)
    , m_resolution(sensor.getResolution())
    , m_maxSampleCount(spp)
    , m_blockStartLUT(computeBlockStartLUT(sensor.getResolution()))
    , m_currentPixelSample(0)
{
    //std::fill(std::begin(m_pixelSampleCount), std::end(m_pixelSampleCount), 0);
}

void SamplerIntegrator::reset()
{
    //std::fill(std::begin(m_pixelSampleCount), std::end(m_pixelSampleCount), 0);
    m_currentPixelSample.store(0);
}

void SamplerIntegrator::render(const PerspectiveCamera& camera)
{
    m_cameraThisFrame = &camera;

    // RAII stopwatch
    auto stopwatch = g_stats.timings.totalRenderTime.getScopedStopwatch();

    // Generate camera rays
#ifndef NDEBUG
    //#if 1
    for (int y = 0; y < m_resolution.y; y++) {
        for (int x = 0; x < m_resolution.x; x++) {
            for (int s = 0; s < m_maxSampleCount; s++) {
                // Initialize camera sample for current sample
                spawnNextSample();
            }
        }
    }
#else
    if constexpr (AccelerationStructure<int>::recurseTillCompletion) {
        // The accelation structure will trace a path untill completion. So we are free to spawn all paths
        // here without concern for memory usage. Recursively spawning paths through spawnNextSample is
        // actually a bad idea because it will lead to stack overflows.
        tbb::blocked_range2d<int, int> sensorRange(0, m_resolution.y, 0, m_resolution.x);
        tbb::parallel_for(sensorRange, [&](tbb::blocked_range2d<int, int> localRange) {
            auto rows = localRange.rows();
            auto cols = localRange.cols();
            for (int y = rows.begin(); y < rows.end(); y++) {
                for (int x = cols.begin(); x < cols.end(); x++) {
                    // Allow for multiple samples (of the same pixel) to be in-flight
                    for (int s = 0; s < m_maxSampleCount; s++) {
                        spawnNextSample(true);
                    }
                }
            }
        });
    } else {
        // SpawnNextSample returns before the full path is traced. So we need to make sure that we do not
        // spawn more rays than the user has requested. The integrator is responsible for calling spawnNextSample
        // when a path ends so that a new path can be spawned.
        tbb::parallel_for(tbb::blocked_range(0, PARALLEL_PATHS), [&](tbb::blocked_range<int> localRange) {
            for (int i = localRange.begin(); i < localRange.end(); i++) {
                spawnNextSampleTillSuccess();
            }
        });
    }
#endif

    m_accelerationStructure.flush();
}

// Keep spawning samples until the camera ray does not miss all geometry. Usefull to prevent
// stack overflow from the recursion caused by the miss shader (calling spawnNextSample) for 
// pixels that do not cover any geometry.
static thread_local bool isProcessingCameraRay = false;
void SamplerIntegrator::spawnNextSampleTillSuccess()
{
    assert(m_cameraThisFrame != nullptr);

    bool success = false;
    do {
        size_t pixelSampleIndex = m_currentPixelSample.fetch_add(1);
        size_t sampleNumber = pixelSampleIndex % m_maxSampleCount;

        size_t globalPixelIndex = pixelSampleIndex / m_maxSampleCount;
        size_t blockIndex = globalPixelIndex / s_pixelsInTile;
        size_t pixelInBlockIndex = globalPixelIndex % s_pixelsInTile;

        if (blockIndex >= m_blockStartLUT.size())
            return;

        glm::ivec2 blockStart = m_blockStartLUT[blockIndex];
        glm::ivec2 pixelInBlock = glm::ivec2(pixelInBlockIndex % s_tileSize, pixelInBlockIndex / s_tileSize);
        glm::ivec2 pixel = blockStart + pixelInBlock;

        if (pixel.x >= m_resolution.x || pixel.y >= m_resolution.y)
            continue;

        unsigned seed = 0;
        if constexpr (USE_RANDOM_SEEDS) {
            static thread_local std::random_device rd = std::random_device();
            seed = rd();
        } else {
            seed = static_cast<unsigned>(pixelSampleIndex);
        }

        // Custom deleter
        // https://stackoverflow.com/questions/12340810/using-custom-deleter-with-stdshared-ptr
        void* samplerSpace = m_samplerAllocator.allocate(1);
        auto* samplerRawPtr = new (samplerSpace) UniformSampler(seed);
        auto samplerPtr = std::shared_ptr<UniformSampler>(samplerRawPtr, [this](UniformSampler* ptr) {
            m_samplerAllocator.deallocate(ptr, 1);
        });

        CameraSample cameraSample = samplerPtr->getCameraSample(pixel);
        RayState rayState {
            { ContinuationRayState { glm::vec3(1.0f), 0, false } },
            pixel,
            samplerPtr
        };

        // Disgusting hack to prevent miss shader of primary rays to spawn new samples when
        // spawnNextSampleTillSuccess is used to spawn new samples. All this trickery is needed
        // because otherwise empty pixels will keep recursing through miss shader -> spawnNextSample
        // resulting in stack overflow.
        // This hack only works if placeIntersectRequestsReturnOnMiss always returns before running a hit shader
        assert(!AccelerationStructure<int>::recurseTillCompletion);// static_assert will never compile with in-core traverser
        isProcessingCameraRay = true;
        Ray ray = m_cameraThisFrame->generateRay(cameraSample);
        m_accelerationStructure.placeIntersectRequestsReturnOnMiss(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
        isProcessingCameraRay = false;
    } while (!success);
}

void SamplerIntegrator::spawnNextSample(bool initialRay)
{
    assert(m_cameraThisFrame != nullptr);

    if constexpr (AccelerationStructure<int>::recurseTillCompletion) {
        if (!initialRay) {
            return;
        }
    } else {
        // Disgusting hack to prevent miss shader of primary rays to spawn new samples when
        // spawnNextSampleTillSuccess is used to spawn new samples. All this trickery is needed
        // because otherwise empty pixels will keep recursing through miss shader -> spawnNextSample
        // resulting in stack overflow.
        if (isProcessingCameraRay) {
            return;
        }
    }

    glm::ivec2 pixel;
    size_t pixelSampleIndex;
    while (true) {
        pixelSampleIndex = m_currentPixelSample.fetch_add(1);
        size_t sampleNumber = pixelSampleIndex % m_maxSampleCount;

        size_t globalPixelIndex = pixelSampleIndex / m_maxSampleCount;
        size_t blockIndex = globalPixelIndex / s_pixelsInTile;
        size_t pixelInBlockIndex = globalPixelIndex % s_pixelsInTile;

        if (blockIndex >= m_blockStartLUT.size())
            return;

        glm::ivec2 blockStart = m_blockStartLUT[blockIndex];
        glm::ivec2 pixelInBlock = glm::ivec2(pixelInBlockIndex % s_tileSize, pixelInBlockIndex / s_tileSize);
        pixel = blockStart + pixelInBlock;

        if (pixel.x >= m_resolution.x || pixel.y >= m_resolution.y)
            continue;

        break;
    }
    /*size_t pixelSampleIndex = m_currentPixelSample.fetch_add(1);
    if (pixelSampleIndex >= m_maxPixelSample)
        return;

    size_t sampleNumber = pixelSampleIndex % m_maxSampleCount;
    size_t pixelIndex = pixelSampleIndex / m_maxSampleCount;
    glm::ivec2 pixel = indexToPixel(pixelIndex);*/

    unsigned seed = 0;
    if constexpr (USE_RANDOM_SEEDS) {
        static thread_local std::random_device rd = std::random_device();
        seed = rd();
    } else {
        seed = static_cast<unsigned>(pixelSampleIndex);
    }

    // Custom deleter
    // https://stackoverflow.com/questions/12340810/using-custom-deleter-with-stdshared-ptr
    void* samplerSpace = m_samplerAllocator.allocate(1);
    auto* samplerRawPtr = new (samplerSpace) UniformSampler(seed);
    auto samplerPtr = std::shared_ptr<UniformSampler>(samplerRawPtr, [this](UniformSampler* ptr) {
        m_samplerAllocator.deallocate(ptr, 1);
    });

    CameraSample cameraSample = samplerPtr->getCameraSample(pixel);
    RayState rayState {
        { ContinuationRayState { glm::vec3(1.0f), 0, false } },
        pixel,
        samplerPtr
    };

    Ray ray = m_cameraThisFrame->generateRay(cameraSample);
    m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
}

void SamplerIntegrator::specularReflect(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& prevRayState)
{
    // Compute specular reflection wi and BSDF value
    glm::vec3 wo = si.wo;
    BxDFType type = BxDFType(BSDF_REFLECTION | BSDF_SPECULAR);
    auto sample = si.bsdf->sampleF(wo, sampler.get2D(), type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    const glm::vec3& ns = si.shading.normal;
    if (sample->pdf > 0.0f && !isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        // TODO: handle ray differentials
        Ray ray = si.spawnRay(sample->wi);

        const auto& prevConstRayState = std::get<ContinuationRayState>(prevRayState.data);
        ContinuationRayState contRayState;
        contRayState.bounces = prevConstRayState.bounces + 1;
        contRayState.weight = prevConstRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        RayState rayState {
            contRayState,
            prevRayState.pixel,
            prevRayState.sampler
        };

        RayState rayStateVariant = rayState;
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayStateVariant, 1));
    }
}

void SamplerIntegrator::specularTransmit(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& prevRayState)
{
    // Compute specular reflection wi and BSDF value
    glm::vec3 wo = si.wo;
    BxDFType type = BxDFType(BSDF_TRANSMISSION);
    auto sample = si.bsdf->sampleF(wo, sampler.get2D(), type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    const glm::vec3& ns = si.shading.normal;
    if (sample->pdf > 0.0f && !isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        // TODO: handle ray differentials
        Ray ray = si.spawnRay(sample->wi);

        const auto& prevConstRayState = std::get<ContinuationRayState>(prevRayState.data);
        ContinuationRayState contRayState;
        contRayState.bounces = prevConstRayState.bounces + 1;
        contRayState.weight = prevConstRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        RayState rayState {
            contRayState,
            prevRayState.pixel,
            prevRayState.sampler
        };

        RayState rayStateVariant = rayState;
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayStateVariant, 1));
    }
}

void SamplerIntegrator::spawnShadowRay(const Ray& ray, bool anyHit, const RayState& prevRayState, const Spectrum& radiance)
{
    const auto& prevConstRayState = std::get<ContinuationRayState>(prevRayState.data);
    ShadowRayState shadowRayState;
    shadowRayState.pixel = prevRayState.pixel;
    shadowRayState.radianceOrWeight = prevConstRayState.weight * radiance;
    shadowRayState.light = nullptr;

    RayState rayState {
        shadowRayState,
        prevRayState.pixel,
        prevRayState.sampler
    };
    if (anyHit) {
        m_accelerationStructure.placeIntersectAnyRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
    } else {
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
    }
}

void SamplerIntegrator::spawnShadowRay(const Ray& ray, bool anyHit, const RayState& prevRayState, const Spectrum& weight, const Light& light)
{
    const auto& prevConstRayState = std::get<ContinuationRayState>(prevRayState.data);
    ShadowRayState shadowRayState;
    shadowRayState.pixel = prevRayState.pixel;
    shadowRayState.radianceOrWeight = prevConstRayState.weight * weight;
    shadowRayState.light = &light;

    RayState rayState {
        shadowRayState,
        prevRayState.pixel,
        prevRayState.sampler
    };

    if (anyHit) {
        m_accelerationStructure.placeIntersectAnyRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
    } else {
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
    }
}

// Compute a Z-order (morton) curve over the screen tiles
std::vector<glm::ivec2> SamplerIntegrator::computeBlockStartLUT(const glm::ivec2& resolution)
{
    uint32_t resolutionX = static_cast<uint32_t>(resolution.x);
    uint32_t resolutionY = static_cast<uint32_t>(resolution.y);

    std::vector<glm::ivec2> ret;
    uint64_t i = 0;
    while (true) {
        uint_fast32_t x, y;
        libmorton::morton2D_64_decode(i++, x, y);

        x *= s_tileSize;
        y *= s_tileSize;

        if (x < resolutionX && y < resolutionY) {
            ret.push_back(glm::ivec2(x, y));
        } else if (x >= resolutionX && y >= resolutionY) {
            break;
        }
    }
    return ret;
}

/*glm::ivec2 SamplerIntegrator::indexToPixel(size_t pixelIndex) const
{
    return glm::ivec2(pixelIndex % m_resolution.x, pixelIndex / m_resolution.x);
}*/

}
