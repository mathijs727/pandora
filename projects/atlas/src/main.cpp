#include "pandora/core/perspective_camera.h"
#include "pandora/core/progressive_renderer.h"
#include "pandora/geometry/sphere.h"
#include "pandora/geometry/triangle.h"
#include "pandora/math/constants.h"
#include "pandora/geometry/scene.h"
#include "ui/fps_camera_controls.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"

#include "pandora/shading/materials/specular_bxdf.h"
#include "pandora/shading/core/bsdf.h"

#include "tbb/tbb.h"
#include <iostream>
#include <pmmintrin.h>
#include <xmmintrin.h>

using namespace pandora;
using namespace atlas;

const int width = 1280;
const int height = 720;

int main()
{
    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    auto si = SurfaceInteraction(nullptr, 0, Point3f(), Vec3f(), Point2f(), Vec3f(), Vec3f(), Normal3f(), Normal3f());
    auto fresnel = DielectricFresnel(1.0f, 1.0f);
    auto myBxDF = SpecularBxDF<decltype(fresnel)>(Spectrum(1.0f), fresnel);
    auto myBSDF = BSDFImpl<decltype(myBxDF), decltype(myBxDF)>(si, 0.0f, myBxDF, myBxDF);
    BSDF* anonymousBSDF = &myBSDF;
    anonymousBSDF->f(Vec3f(), Vec3f(), BxDF_ALL);

#ifdef WIN32
    std::string projectBasePath = "C:/Users/Mathijs/Documents/GitHub/pandora/";
#else
    std::string projectBasePath = "../";
#endif

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer;
    frameBuffer.clear(Vec3f(0.8f, 0.5f, 0.3f));

    float aspectRatio = static_cast<float>(width) / height;
    PerspectiveCamera camera = PerspectiveCamera(aspectRatio, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(Vec3f(0.0f, 0.5f, -4.0f));
    //camera.setOrientation(QuatF::rotation(Vec3f(0, 1, 0), piF * 1.0f));

    //Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.8f);
    //auto mesh = TriangleMesh::singleTriangle();
    auto mesh = TriangleMesh::loadFromFile(projectBasePath + "assets/monkey.obj");
    //auto mesh = TriangleMesh::loadFromFile(projectBasePath + "assets/cornell_box.obj");
    if (mesh == nullptr) {
#ifdef WIN32
        system("PAUSE");
#endif
        exit(1);
    }

	Scene scene;
	scene.addShape(mesh.get());
	ProgressiveRenderer renderer(1280, 720, scene);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    while (!myWindow.shouldClose() && !pressedEscape) {
		renderer.clear();

        myWindow.updateInput();
        cameraControls.tick();

        auto prevFrameEndTime = std::chrono::high_resolution_clock::now();
		renderer.incrementalRender(camera);
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - prevFrameEndTime);
        prevFrameEndTime = now;
        std::cout << "Time to render frame: " << timeDelta.count() / 1000.0f << " miliseconds" << std::endl;

        frameBuffer.update(renderer.getSensor());
        myWindow.swapBuffers();
    }

    return 0;
}