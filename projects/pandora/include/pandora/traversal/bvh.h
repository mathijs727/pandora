#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/geometry/triangle.h"
#include "pandora/math/bounds3.h"
#include "pandora/math/mat3x4.h"
#include "pandora/utility/blocked_stack_allocator.h"
#include <cassert>
#include <vector>

namespace pandora {

struct BvhPrimitive {
    uint32_t geomID;
    uint32_t primID;
};

template <int N>
class BVH {
public:
    struct NodeRef;
    struct InternalNode;
    struct TransformNode;

    struct NodeRef {
    private:
        static constexpr size_t alignmentBits = 5; // 32 byte aligned

        static constexpr size_t typeBits = 2;
        static constexpr size_t primCountBits = 3;
        static constexpr size_t pointerBits = 64 - typeBits - primCountBits;

        static_assert(typeBits + primCountBits <= alignmentBits, "Prim+type bits take too much space");

        enum NodeType : uint64_t {
            InternalNodeType = 0u,
            TransformNodeType = 1u,
            LeafNodeType = 2u,
            UnknownNodeType = 3u
        };

    public:
        NodeRef()
            : m_type(0)
            , m_primCount(0)
            , m_ptr(0)
        {
        }

        NodeRef(InternalNode* node)
        {

            // Cast to size_t and then uint64_t so this cast wont fail when compiling in 32 bit mode
            uint64_t ptr64 = static_cast<uint64_t>(reinterpret_cast<size_t>(node));
            assert((((1 << alignmentBits) - 1) & ptr64) == 0);
            ptr64 >>= alignmentBits;

            m_type = InternalNodeType;
            m_primCount = 0;
            m_ptr = ptr64;
        }

        NodeRef(BvhPrimitive* primitives, uint64_t numPrims)
        {
            // Cast to size_t and then uint64_t so this cast wont fail when compiling in 32 bit mode
            uint64_t ptr64 = static_cast<uint64_t>(reinterpret_cast<size_t>(primitives));
            assert((((1 << alignmentBits) - 1) & ptr64) == 0);
            ptr64 >>= alignmentBits;

            m_type = LeafNodeType;
            m_primCount = numPrims;
            m_ptr = ptr64;
        }

        bool isInternalNode() const { return m_type == InternalNodeType; }
        bool isTransformNode() const { return m_type == TransformNodeType; }
        bool isLeaf() const { return m_type == LeafNodeType; }

        InternalNode* getInternalNode() const
        {
            assert(isInternalNode());
            return reinterpret_cast<InternalNode*>(m_ptr << alignmentBits);
        }
        TransformNode* getTransformNode() const
        {
            assert(isTransformNode());
            return reinterpret_cast<InternalNode*>(m_ptr << alignmentBits);
        }
        BvhPrimitive* getPrimitives() const
        {
            return reinterpret_cast<BvhPrimitive*>(m_ptr << alignmentBits);
        }

        uint64_t numPrimitives() const
        {
            return m_primCount;
        }

    private:
        uint64_t m_type : typeBits;
        uint64_t m_primCount : primCountBits;
        uint64_t m_ptr : pointerBits;
    };

    struct InternalNode {
        NodeRef children[N];
        Bounds3f childBounds[N];
    };

    struct TransformNode {
        Mat3x4f local2world;
        Mat3x4f world2local;
        Bounds3f localBounds;
        NodeRef child;
    };

public:
    NodeRef m_rootNode;
    std::vector<const TriangleMesh*> m_shapes;

    // TODO: Use a fast (thread local) allocator so we build the BVH on multiple threads AND
    //       to prevent reallocation as the tree grows (plus we dont want to ask the OS for such
    //       a massive block of conginuous memory)
    BlockedStackAllocator m_nodeAllocator;
    BlockedStackAllocator m_primitiveAllocator;
};
}
