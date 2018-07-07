# Pandora [![Build Status](https://travis-ci.com/mathijs727/pandora.svg?token=BHkWQ9P5pzBfP88jbtB8&branch=master)](https://travis-ci.com/mathijs727/pandora)  
Pandora is an out-of-core path tracer developed for my master thesis at Utrecht University / Delft Technical University. The idea for this project originates from my internship at Walt Disney Animation Studios where I I worked on their in-house production path tracer Hyperion.

This project aims at improving the rendering performance of existing worked based on memory-coherent ray tracing (Rendering Complex Scenes with Memory-Coherent Ray Tracing by Pharr et al). The acceleration structure consists of a two level BVH scheme. Leaf nodes of the top level BVH (containing geometry & a bottom level BVH) can be stored to disk when memory runs out. The goal of this project is to experiment with storing a simplified representation of this geometry and using it as an early-out test for rays passing through the bounding volume of a top level leaf node. Additionally, the approximate intersection points found may be used to sort rays for extra coherence.

A lot of parts of the code are directly based on, or inspired by [PBRTv3](https://github.com/mmp/pbrt-v3) and the corresponding book ([Physically Based Rendering from Theory to Implementation, Third Edition](http://www.pbrt.org/)). The bottom level BVH traversal code is based on [Accelerated single ray tracing for wide vector units](https://dl.acm.org/citation.cfm?id=3105785) (Embree's traversal kernels cannot be used because they do not support storing the BVH to disk). The top level BVH traversal will be based on the following work: [Fast Divergent Ray Traversal by Batching Rays in a BVH](https://dspace.library.uu.nl/handle/1874/343844). The early-out testing geometry will be represented using a [Sparse Voxel DAG](https://dl.acm.org/citation.cfm?id=2462024).

## Dependencies
To build Pandora, CMake and a C++17 compiler are required. The user is responsible for installing all the required libraries except EASTL and mio.

Pandora uses the following third-party libraries:
 - [Guideline Support Library](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md) ([implemented by Microsoft](https://github.com/Microsoft/GSL))
 - [TBB](https://github.com/01org/tbb)
 - [Embree 3](https://embree.github.io)
 - [GLM](https://github.com/g-truc/glm)
 - [Assimp](https://github.com/assimp/assimp)
 - [OpenImageIO](https://github.com/OpenImageIO/oiio)
 - [EASTL](https://github.com/electronicarts/EASTL) (bundled)
 - [mio](https://github.com/mandreyel/mio) (bundled)
 - [flatbuffers](git@github.com:google/flatbuffers.git)
 - [Google Test](https://github.com/google/googletest) (only when tests are enabled)

In addition Atlas, the real-time viewer, requires:
 - [GLFW3](http://www.glfw.org/)
 - [GLEW](http://glew.sourceforge.net/)
 - OpenGL

### Windows

On Windows it is recommended to use [vcpkg](https://github.com/Microsoft/vcpkg) to install all required dependencies except Embree (until vcpkg is updated to contain Embree 3).

To get an updated version of Embree, simply build it from source using TBB installed through vcpkg:

```bash
vcpkg install tbb:x64-windows
git clone git@github.com:embree/embree.git
cd embree
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DEMBREE_ISPC_SUPPORT=OFF -DEMBREE_TBB_ROOT="/path_to_vcpkg/installed/x64-windows" ../
ninja
ninja install
```

### Arch Linux (and Arch based distros)

All dependencies can be installed through pacman and yaourt (to access the Arch User Repository). Travis currently also uses Arch Linux (in a Docker container) to install all the dependencies.