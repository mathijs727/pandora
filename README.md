# Pandora
Pandora is an out-of-core path tracer that I originally developed for my master thesis titled "Occlusion Culling in Batched Ray Traversal" (see [thesis.pdf](https://github.com/mathijs727/pandora/blob/master/thesis.pdf) in the root of the repository). This code was further refined for the Eurographics short paper "Conservative Ray Batching using Geometry Proxies" (link coming soon).

This project aims to improve performance of batched ray traversal ("Rendering Complex Scenes with Memory-Coherent Ray Tracing" [Pharr et al. 1997]) for out-of-core path tracing. The acceleration structure consists of a two level BVH scheme. Leaf nodes of the top level BVH (containing geometry & a bottom level BVH) are stored on disk with an in-memory cache. Ray batching is applied at each top level leaf node such that the cost of loading the underlying data may be amortized over many rays. The goal of this project is to experiment with storing a simplified conservative representation of the geometry and using it as an early test for rays reaching a top level leaf node. The approximate intersection points could also be used to sort rays for extra coherence (not implemented yet).

Most parts of the code are directly based on, or inspired by, [PBRTv3](https://github.com/mmp/pbrt-v3) and the corresponding book ([Physically Based Rendering from Theory to Implementation, Third Edition](http://www.pbrt.org/)). The bottom level BVHs use the [Intel Embree library](https://www.embree.org/api.html) however an implementation of [Accelerated single ray tracing for wide vector units](https://dl.acm.org/citation.cfm?id=3105785) is also provided for testing BVHs that are stored/loaded from disk (which Embree does not support). The top level BVH traversal is based on the following work: [Fast Divergent Ray Traversal by Batching Rays in a BVH](https://dspace.library.uu.nl/handle/1874/343844). The (conservative) voxelization and SVO construction code are implementated based on the  algorithms presented in [Fast Parallel Surface and Solid Voxelization on GPUs](http://research.michael-schwarz.com/publ/files/vox-siga10.pdf). SVO to SVDAG compression is an implementation of [
Exploiting self-similarity in geometry for voxel based solid modeling](https://dl.acm.org/citation.cfm?id=781631). Finally, SVDAG traversal was implemented using SSE instructions based on a stripped down version of the GPU traversal algorithm presented in [Efficient Sparse Voxel Octrees](https://research.nvidia.com/publication/efficient-sparse-voxel-octrees).

Both the command line interface (Torque) and the real-time viewer (Atlas) support loading "\*.pbrt" and "\*.pbf" (generated using [Ingo Walds pbrt parser](https://github.com/ingowald/pbrt-parser)) files.

## Dependencies
Pandora rellies on a significant amount of third-party libraries. We recommend the usage of [vcpkg](https://github.com/microsoft/vcpkg) to install all requires packages on both Windows and Linux.

Pandora uses the following third-party libraries:
 - [assimp](https://github.com/assimp/assimp)
 - [Boost Program Options](https://www.boost.org/)
 - [EASTL](https://github.com/electronicarts/EASTL)
 - [Compositional Numerical Library (CNL)](https://github.com/johnmcfarlane/cnl)
 - [Guideline Support Library](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md) ([implemented by Microsoft](https://github.com/Microsoft/GSL))
 - [Embree](https://embree.github.io)
 - [A Modern Formatting Library (fmt)](https://github.com/fmtlib/fmt)
 - [GLM](https://github.com/g-truc/glm)
 - [Google Test](https://github.com/google/googletest) (only when tests are enabled)
 - [mio](https://github.com/mandreyel/mio)
 - [nlohmann-json](https://github.com/nlohmann/json)
 - [spdlog](https://github.com/gabime/spdlog)
 - [Intel Threaded Building Blocks](https://github.com/01org/tbb)
 - [OpenImageIO](https://github.com/OpenImageIO/oiio)
 - [flatbuffers](https://github.com/google/flatbuffers)
 - [libmorton](https://github.com/Forceflow/libmorton)

In addition Atlas, the real-time viewer, requires:
 - [GLFW3](http://www.glfw.org/)
 - [GLEW](http://glew.sourceforge.net/)
 - OpenGL

Install using vcpkg:
`vcpkg install assimp boost-program-options eabase eastl cnl ms-gsl embree3 fmt glm gtest mio nlohmann-json spdlog tbb openimageio flatbuffers libmorton glfw3 glew`

## Projects
The code base is divided into several smaller projects such that code can be reused more easily:

- atlas:		``real-time'' viewer based on Pandora
- diskbench:	very simple file (read) performance benchmark (to diagnose file caching)
- metrics:		a simple metrics library that supports outputting run-time data to a JSON file
- **pandora**:	library containing the core renderer code
- simd:		an collection of (hand written) SSE/AVX wrapper classes
- Torque:		command-line (offline) renderer based on Pandora
- Voxel:		mini project to test and experiment with the voxelization code
