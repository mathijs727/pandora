#include "pandora/geometry/triangle.h"
#include "pandora/samplers/uniform_sampler.h"
#include "gtest/gtest.h"
#include <random>
#include <thread>
#include <vector>

using namespace pandora;

#define MSVC (_MSC_VER && !__INTEL_COMPILER)

TEST(Triangle, Intersect1)
{
    std::vector<glm::ivec3> triangles = { glm::ivec3(0, 1, 2) };
    std::vector<glm::vec3> positions = { glm::vec3(-2, 0, 0), glm::vec3(0, 2, 0), glm::vec3(2, 0, 0) };

    auto trianglesPtr = std::make_unique<glm::ivec3[]>(triangles.size());
    auto positionsPtr = std::make_unique<glm::vec3[]>(positions.size());
#if MSVC
	// Prevent MSVC from nagging about unsafe copy
	std::copy(std::begin(triangles), std::end(triangles), stdext::checked_array_iterator<glm::ivec3*>(trianglesPtr.get(), triangles.size()));
	std::copy(std::begin(positions), std::end(positions), stdext::checked_array_iterator<glm::vec3*>(positionsPtr.get(), positions.size()));
#else
    std::copy(std::begin(triangles), std::end(triangles), trianglesPtr.get());
    std::copy(std::begin(positions), std::end(positions), positionsPtr.get());
#endif

    TriangleMesh mesh((unsigned)triangles.size(), (unsigned)positions.size(), std::move(trianglesPtr), std::move(positionsPtr), nullptr, nullptr, nullptr);
    {
        Ray ray = Ray(glm::vec3(0, 0, -1), glm::vec3(0, 0, 1));
        float tHit;
        SurfaceInteraction isect;
        ASSERT_TRUE(mesh.intersectPrimitive(0, ray, tHit, isect));
    }
    {
        Ray ray = Ray(glm::vec3(0, 0, 1), glm::vec3(0, 0, -1));
        float tHit;
        SurfaceInteraction isect;
        ASSERT_TRUE(mesh.intersectPrimitive(0, ray, tHit, isect));
    }
    {
        Ray ray = Ray(glm::vec3(0, 3, -1), glm::vec3(0, 0, 1));
        float tHit;
        SurfaceInteraction isect;
        ASSERT_FALSE(mesh.intersectPrimitive(0, ray, tHit, isect));
    }
    {
        Ray ray = Ray(glm::vec3(-1.0f, -0.5f, -1), glm::normalize(glm::vec3(1, 1, 1)));
        float tHit;
        SurfaceInteraction isect;
        ASSERT_TRUE(mesh.intersectPrimitive(0, ray, tHit, isect));
    }

    {
        Ray ray = Ray(glm::vec3(0.99f, 0.99f, -1), glm::vec3(0, 0, 1));
        float tHit;
        SurfaceInteraction isect;
        ASSERT_TRUE(mesh.intersectPrimitive(0, ray, tHit, isect));
    }
}

TEST(Triangle, AreaCorrect)
{
    std::vector<glm::ivec3> triangles = { glm::ivec3(0, 1, 2) };
    std::vector<glm::vec3> positions = { glm::vec3(-2, 0, 0), glm::vec3(0, 0, 2), glm::vec3(2, 0, 0) };

    auto trianglesPtr = std::make_unique<glm::ivec3[]>(triangles.size());
    auto positionsPtr = std::make_unique<glm::vec3[]>(positions.size());
#if MSVC
	// Prevent MSVC from nagging about unsafe copy
	std::copy(std::begin(triangles), std::end(triangles), stdext::checked_array_iterator<glm::ivec3*>(trianglesPtr.get(), triangles.size()));
	std::copy(std::begin(positions), std::end(positions), stdext::checked_array_iterator<glm::vec3*>(positionsPtr.get(), positions.size()));
#else
	std::copy(std::begin(triangles), std::end(triangles), trianglesPtr.get());
	std::copy(std::begin(positions), std::end(positions), positionsPtr.get());
#endif

    TriangleMesh mesh((unsigned)triangles.size(), (unsigned)positions.size(), std::move(trianglesPtr), std::move(positionsPtr), nullptr, nullptr, nullptr);

    float scanLine = 1.0f;
    int totalSampleCount = 0;
    int samplesHit = 0;
    for (float x = -2.0f; x < 2.0f; x += 0.01f) {
        Ray ray = Ray(glm::vec3(x, -1, scanLine), glm::vec3(0, 1, 0));
        float tHit;
        SurfaceInteraction isect;
        if (mesh.intersectPrimitive(0, ray, tHit, isect))
            samplesHit++;
        totalSampleCount++;
    }
    ASSERT_EQ(samplesHit, totalSampleCount / 2);
}

TEST(Triangle, Sample)
{
	UniformSampler sampler(1);
    std::vector<glm::ivec3> triangles = { glm::ivec3(0, 1, 2), glm::ivec3(3, 4, 5) };
	std::vector<glm::vec3> positions;
	for (int i = 0; i < 6; i++) {
		positions.push_back(glm::vec3(sampler.get1D(), sampler.get1D(), sampler.get1D()));
	}

    auto trianglesPtr = std::make_unique<glm::ivec3[]>(triangles.size());
    auto positionsPtr = std::make_unique<glm::vec3[]>(positions.size());
#if MSVC
	// Prevent MSVC from nagging about unsafe copy
	std::copy(std::begin(triangles), std::end(triangles), stdext::checked_array_iterator<glm::ivec3*>(trianglesPtr.get(), triangles.size()));
	std::copy(std::begin(positions), std::end(positions), stdext::checked_array_iterator<glm::vec3*>(positionsPtr.get(), positions.size()));
#else
	std::copy(std::begin(triangles), std::end(triangles), trianglesPtr.get());
	std::copy(std::begin(positions), std::end(positions), positionsPtr.get());
#endif

    TriangleMesh mesh((unsigned)triangles.size(), (unsigned)positions.size(), std::move(trianglesPtr), std::move(positionsPtr), nullptr, nullptr, nullptr);

	for (int p = 0; p < 2; p++)
	{
		for (int i = 0; i < 100; i++) {
			auto interaction = mesh.samplePrimitive(p, sampler.get2D());
			ASSERT_FLOAT_EQ(glm::length(interaction.normal), 1.0f);

			Ray ray = Ray(interaction.position - interaction.normal, interaction.normal);
			float tHit;
			SurfaceInteraction isect;
			ASSERT_TRUE(mesh.intersectPrimitive(p, ray, tHit, isect));
		}
	}
}

TEST(Triangle, SampleFromReference)
{
	UniformSampler sampler(1);
	std::vector<glm::ivec3> triangles = { glm::ivec3(0, 1, 2), glm::ivec3(3, 4, 5) };
	std::vector<glm::vec3> positions;
	for (int i = 0; i < 6; i++) {
		positions.push_back(glm::vec3(sampler.get1D(), sampler.get1D(), sampler.get1D()));
	}

	auto trianglesPtr = std::make_unique<glm::ivec3[]>(triangles.size());
	auto positionsPtr = std::make_unique<glm::vec3[]>(positions.size());
#if MSVC
	// Prevent MSVC from nagging about unsafe copy
	std::copy(std::begin(triangles), std::end(triangles), stdext::checked_array_iterator<glm::ivec3*>(trianglesPtr.get(), triangles.size()));
	std::copy(std::begin(positions), std::end(positions), stdext::checked_array_iterator<glm::vec3*>(positionsPtr.get(), positions.size()));
#else
	std::copy(std::begin(triangles), std::end(triangles), trianglesPtr.get());
	std::copy(std::begin(positions), std::end(positions), positionsPtr.get());
#endif

	TriangleMesh mesh((unsigned)triangles.size(), (unsigned)positions.size(), std::move(trianglesPtr), std::move(positionsPtr), nullptr, nullptr, nullptr);

	Interaction ref;
	ref.position = glm::vec3(-23, 45, 34);
	ref.wo = glm::normalize(glm::vec3(1, 3, -2));

	for (int p = 0; p < 2; p++)
	{
		for (int i = 0; i < 100; i++) {
			auto interaction = mesh.samplePrimitive(p, ref, sampler.get2D());
			ref.normal = glm::normalize(interaction.position - ref.position);// Normal oriented towards the light
			ASSERT_FLOAT_EQ(glm::length(interaction.normal), 1.0f);

			Ray ray = ref.spawnRay(glm::normalize(interaction.position - ref.position));
			float tHit;
			SurfaceInteraction isect;
			ASSERT_TRUE(mesh.intersectPrimitive(p, ray, tHit, isect));

			ASSERT_NE(mesh.pdfPrimitive(p, ref, glm::normalize(interaction.position - ref.position)), 0.0f);
		}
	}
}
