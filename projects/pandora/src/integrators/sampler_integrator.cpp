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
    , m_maxPixelSample((size_t)m_resolution.x * (size_t)m_resolution.y * (size_t)spp)
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
            for (int s = 0; s < m_parallelSamples; s++) {
                // Initialize camera sample for current sample
                auto pixel = glm::ivec2(x, y);
                spawnNextSample(pixel);
            }
        }
    }
#else
    if constexpr (AccelerationStructure<int>::recurseTillCompletion) {
        // The accelation structure will trace a path untill completion. So we are free to spawn all paths
        // here without concern for memory usage. Recursively spawning paths through spawnNextSample is
        // actually a bad idea in this case because it will lead to stack overflows.
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
                spawnNextSample();
            }
        });
    }
#endif

    m_accelerationStructure.flush();
}

void SamplerIntegrator::spawnNextSample(bool initialRay)
{
    assert(m_cameraThisFrame != nullptr);

    if constexpr (AccelerationStructure<int>::recurseTillCompletion) {
        if (!initialRay) {
            return;
        }
    }

    size_t pixelSampleIndex = m_currentPixelSample.fetch_add(1);
    if (pixelSampleIndex >= m_maxPixelSample)
        return;

    size_t sampleNumber = pixelSampleIndex % m_maxSampleCount;
    size_t pixelIndex = pixelSampleIndex / m_maxSampleCount;
    glm::ivec2 pixel = indexToPixel(pixelIndex);

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

glm::ivec2 SamplerIntegrator::indexToPixel(size_t pixelIndex) const
{
    return glm::ivec2(pixelIndex % m_resolution.x, pixelIndex / m_resolution.x);
}

}
