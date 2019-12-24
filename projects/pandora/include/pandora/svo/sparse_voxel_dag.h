#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#ifdef PANDORA_ISPC_SUPPORT
#include "sparse_voxel_dag_traversal16_ispc.h"
#include "sparse_voxel_dag_traversal32_ispc.h"
#endif
#include "simd/intrinsics.h"
#include <EASTL/fixed_vector.h>
#include <glm/glm.hpp>
#include <gsl/span>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace pandora {

class SparseVoxelDAG {
public:
    SparseVoxelDAG(const VoxelGrid& grid);
    SparseVoxelDAG(SparseVoxelDAG&&) = default;
    ~SparseVoxelDAG() = default;
    SparseVoxelDAG& operator=(SparseVoxelDAG&&) = default;

    static void compressDAGs(gsl::span<SparseVoxelDAG*> svos);

#ifdef PANDORA_ISPC_SUPPORT
    void intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const;
#endif
    std::optional<float> intersectScalar(Ray ray) const;
    void testSVDAG() const;

    std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

    size_t sizeBytes() const;

private:
    using NodeOffset = uint32_t; // Either uint32_t or uint16_t
    //using AbsoluteNodeOffset = size_t;

    // NOTE: child pointers are stored directly after the descriptor
    struct Descriptor {
        uint8_t leafMask;
        uint8_t validMask;

        inline bool isEmpty() const { return validMask == 0x0; }; // Completely empty node
        //inline bool isFilledLeaf() const { return (validMask & leafMask) == 0xFF; }; // Completely filled leaf node
        inline bool isValid(int i) const { return validMask & (1 << i); };
        inline bool isLeaf(int i) const { return (validMask & leafMask) & (1 << i); };
        inline bool isInnerNode(int i) const { return (validMask & (~leafMask)) & (1 << i); };
        inline int numInnerNodeChildren() const { return simd::popcount32(validMask & (~leafMask)); };
        inline int numChildren() const { return simd::popcount32(validMask); };

        Descriptor() = default;
        //inline explicit Descriptor(const SVOChildDescriptor& v) { leafMask = v.leafMask; validMask = v.validMask; __padding = 0; };
        //inline explicit Descriptor(RelativeNodeOffset v) { memcpy(this, &v, sizeof(Descriptor)); };
        inline explicit operator NodeOffset() const { return static_cast<NodeOffset>(*reinterpret_cast<const uint16_t*>(this)); };
        inline bool operator==(const Descriptor& other) const { return leafMask == other.leafMask && validMask == other.validMask; };
    };
    static_assert(sizeof(Descriptor) == sizeof(uint16_t));

    NodeOffset constructSVOBreadthFirst(const VoxelGrid& grid);
    static Descriptor createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask);

    const Descriptor* getChild(const Descriptor* descriptor, int idx) const;

private:
    unsigned m_resolution;
    glm::vec3 m_boundsMin;
    glm::vec3 m_boundsExtent;
    glm::vec3 m_invBoundsExtent;

    //std::vector<std::pair<size_t, size_t>> m_treeLevels;
    NodeOffset m_rootNodeOffset;
    std::vector<NodeOffset> m_nodeAllocator;
    const NodeOffset* m_data;
};

}
