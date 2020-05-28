# Pandora [![Build Status](https://travis-ci.com/mathijs727/pandora.svg?token=BHkWQ9P5pzBfP88jbtB8&branch=master)](https://travis-ci.com/mathijs727/pandora)  
Pandora is an out-of-core path tracer developed for my master thesis titled "Occlusion Culling in Batched Ray Traversal" (see [thesis.pdf](https://github.com/mathijs727/pandora/blob/master/thesis.pdf) in the root of the repository). This project took place under the supervision of Elmar Eisemann and Markus Billeter from the Delft University of Technology and Jacco Bikker from Utrecht University. The idea for this project originates from my internship at Walt Disney Animation Studios where I I worked on their in-house production path tracer Hyperion.

This project aims to improve performance of batched ray traversal ("Rendering Complex Scenes with Memory-Coherent Ray Tracing" [Pharr et al. 1997]) for the use out-of-core ray traversal. The acceleration structure consists of a two level BVH scheme. Leaf nodes of the top level BVH (containing geometry & a bottom level BVH) are stored on disk with an in-memory cache. Ray batching is applied at each leaf node such that the cost of loading the underlying data may be amortized over many rays. The goal of this project is to experiment with storing a simplified conservative representation of the geometry and using it as an early-out test for rays hitting the bounding volume of a top level leaf node. Additionally, the approximate intersection points could be used to sort rays for extra coherence (this is not implemented yet).

Some parts of the code are directly based on, or inspired by, [PBRTv3](https://github.com/mmp/pbrt-v3) and the corresponding book ([Physically Based Rendering from Theory to Implementation, Third Edition](http://www.pbrt.org/)). The bottom level BVH traversal code (using AVX2) is based on [Accelerated single ray tracing for wide vector units](https://dl.acm.org/citation.cfm?id=3105785) (Embree's traversal kernels cannot be used because they do not support storing the BVH to disk). The top level BVH traversal (using SSE) is based on the following work: [Fast Divergent Ray Traversal by Batching Rays in a BVH](https://dspace.library.uu.nl/handle/1874/343844). The (conservative) voxelization and SVO construction code are implementated based on the  algorithms presented in [Fast Parallel Surface and Solid Voxelization on GPUs](http://research.michael-schwarz.com/publ/files/vox-siga10.pdf). SVO to SVDAG compression is an implementation of [
Exploiting self-similarity in geometry for voxel based solid modeling](https://dl.acm.org/citation.cfm?id=781631). Finally, SVDAG traversal was implemented using SSE instructions based on a stripped down version of the GPU traversal algorithm presented in [Efficient Sparse Voxel Octrees](https://research.nvidia.com/publication/efficient-sparse-voxel-octrees).

## Dependencies
Pandora rellies on a significant amount of third-party libraries. We recommend the usage of [vcpkg](https://github.com/microsoft/vcpkg) to download and install the required packages. The current build of Pandora was tested using vcpkg revision `d9b4acf02d8c784927f89d23edb677408ec428af`.

### Building on Windows

All dependencies will be downloaded and build automatically. Users that have vcpkg installed and are using Visual Studio with CMake integration should make sure to set `CMAKE_TOOLCHAIN_FILE=""` in Visual Studio's CMake settings file (`CMakeSettings.json`).

### Building on Linux

At the time of writing vcpkg cannot build `hpx` because vcpkg does support building `hwloc` on Linux. To work around this issue we provide a modified portfile on Linux which disables the hwloc dependency. Linux users are required to install `hwloc` and `TCMalloc` manually:
`sudo apt install libhwloc-dev libgoogle-perftools-dev`

Pandora uses the following third-party libraries:
 - [Guideline Support Library](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md) ([implemented by Microsoft](https://github.com/Microsoft/GSL))
 - [HPX](https://github.com/STEllAR-GROUP/hpx)
 - [TBB](https://github.com/01org/tbb)
 - [Embree](https://embree.github.io)
 - [GLM](https://github.com/g-truc/glm)
 - [assimp](https://github.com/assimp/assimp)
 - [tinyply](https://github.com/ddiakopoulos/tinyply)
 - [OpenImageIO](https://github.com/OpenImageIO/oiio)
 - [EASTL](https://github.com/electronicarts/EASTL) (bundled)
 - [mio](https://github.com/mandreyel/mio) (bundled)
 - [flatbuffers](https://github.com/google/flatbuffers)
 - [libmorton](https://github.com/Forceflow/libmorton)
 - [Boost Hash](https://www.boost.org/)
 - [Boost TTI](https://www.boost.org/) (until concepts make it into the language)
 - [Google Test](https://github.com/google/googletest) (only when tests are enabled)

Runtime metrics:
 - [nlohmann-json](https://github.com/nlohmann/json)

In addition Atlas, the real-time viewer, requires:
 - [GLFW3](http://www.glfw.org/)
 - [GLEW](http://glew.sourceforge.net/)
 - OpenGL


## Projects
The code base is divided into several smaller projects such that code can be reused more easily:

- atlas:		``real-time'' viewer based on Pandora
- diskbench:	very simple file (read) performance benchmark (to diagnose file caching)
- metrics:		a simple metrics library that supports outputting run-time data to a JSON file
- **pandora**:	library containing the core renderer code
- simd:		an collection of (hand written) SSE/AVX wrapper classes
- Torque:		command-line (offline) renderer based on Pandora
- Voxel:		mini project to test and experiment with the voxelization code


## Notes
Rendering scenes out-of-core by lowering the goemetry limit will not actually access the disk if the computer contains enough RAM to store the whole scene. The operating system will cache recently accessed files, resulting in subsequent accesses to be a simple memcpy from a system buffer. This is much faster than actual disk reads so be aware when benchmarking. In an effort to disable this behavior, Pandora optionally uses Direct I/O to bypass this file cache (see ```pandora/projects/pandora/include/pandora/utility/file_batcher.h```). This feature can be toggled in the config header file (```pandora/projects/pandora/include/pandora/```).

Your disk may also apply caching (in a RAM or SLC NAND cache). As far as I know this cannot be disabled in any way.