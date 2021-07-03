# Conservative Ray Batching using Geometry Proxies
Pandora is an out-of-core path tracer developed the [Conservative Ray Batching using Geometry Proxies](https://diglib.eg.org/handle/10.2312/egs20201006) short paper that was presented at Eurographics 2020.
More information, including a preprint, is available at: [https://graphics.tudelft.nl/Publications-new/2020/ME20/](https://graphics.tudelft.nl/Publications-new/2020/ME20/).

Originally developed as a Master thesis, this project aims to improve performance of batched ray traversal ("Rendering Complex Scenes with Memory-Coherent Ray Tracing" [Pharr et al. 1997]) for the use out-of-core ray traversal.
The acceleration structure consists of a two level BVH scheme.
Leaf nodes of the top level BVH (containing geometry & bottom level BVH) are stored on disk with an in-memory cache.
Ray batching is applied at each leaf node such that the cost of loading the underlying data may be amortized over many rays.
The goal of this project is to experiment with storing a simplified proxy geometry for each batching point and using it as an early-out test for rays.
Additionally, the approximate intersection points could be used to sort rays for extra coherence (this is not implemented yet).

Some parts of the code are directly based on, or inspired by, [PBRTv3](https://github.com/mmp/pbrt-v3) and the corresponding book ([Physically Based Rendering from Theory to Implementation, Third Edition](http://www.pbrt.org/)) which is now [available online for free](https://www.pbr-book.org/).
The bottom level BVH traversal code either uses Embree or an AVX2 implementation of [Accelerated single ray tracing for wide vector units](https://dl.acm.org/citation.cfm?id=3105785) which can be stored on disk instead of having to be rebuild (code can be found in `projects/pandora/include/pandora/traversal/bvh/wive_bvh8_XXX.h`).
The top level BVH (referred to as `PausableBVH` in code) uses a SSE implementation of: [Fast Divergent Ray Traversal by Batching Rays in a BVH](https://dspace.library.uu.nl/handle/1874/343844).
Finally, the proxy geometry is created using the [Fast Parallel Surface and Solid Voxelization on GPUs](http://research.michael-schwarz.com/publ/files/vox-siga10.pdf) algorithm and stored as a Sparse Voxel Directed Acyclic Graph,
which are traverersed using a stripped down version of algorithm presented in [Efficient Sparse Voxel Octrees](https://research.nvidia.com/publication/efficient-sparse-voxel-octrees).


## Building
Pandora relies on a significant amount of third-party libraries.
We recommend the use of [vcpkg](https://github.com/microsoft/vcpkg) to download and install the required packages.
A `vcpkg.json` manifest file is provided such that all dependencies will automatically be installed when `vcpkg` is available (see [link](https://github.com/microsoft/vcpkg) on how to use vcpkg).


## Acceleration Structures
There are three different acceleration structures between one can choose by modifying `main.cpp`:

- `EmbreeAccelerationStructureBuilder`:				wrapper around Embree with no out-of-core support
- `BatchingAccelerationStructureBuilder`:			two-level hierarchy using Embree for the bottom level (rebuild instead of loading from disk)
- `OfflineBatchingAccelerationStructureBuilder`:	two-level hierarchy with a bottom level structure which can be stored/loaded from disk


## Projects
The code base is divided into several smaller projects such that code can be reused more easily:

- **atlas**:			``real-time'' viewer based on Pandora
- diskbench:		very simple file (read) performance benchmark (to diagnose file caching)
- metrics:			a simple metrics library that supports outputting run-time data to a JSON file
- **pandora**:		library containing the core renderer code
- pbf_importer:		parser of the unofficial [Binary PBRT File(PBF)](https://github.com/ingowald/pbrt-parser) format
- pbrt_importer2:	parser of the [PBRTv3 file format](https://www.pbrt.org/fileformat-v3)
- simd:				collection of (hand written) SSE/AVX wrapper classes
- stream_framework:	small framework for parallel stream processing using a high performance queue.
- **torque**:		command-line (offline) renderer based on Pandora
- voxel:			mini project to test and experiment with the voxelization code


## Notes
Rendering scenes out-of-core by lowering the goemetry limit will not actually access the disk if the computer contains enough RAM to store the whole scene.
The operating system will cache recently accessed files, resulting in subsequent accesses to be a simple memcpy from a system buffer.
This is much faster than actual disk reads so be aware when benchmarking.
In an effort to disable this behavior, Pandora optionally uses Direct I/O to bypass this file cache (see ```pandora/projects/pandora/include/pandora/utility/file_batcher.h```).
This feature can be toggled in the config header file (```pandora/projects/pandora/include/pandora/```).

Your disk may also apply caching (e.g. SRAM or SLC NAND cache), this cannot be disabled.
