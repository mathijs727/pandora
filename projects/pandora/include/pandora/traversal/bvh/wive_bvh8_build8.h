#pragma once
#include "wive_bvh8.h"
#include <variant>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8Build8 : public WiVeBVH8<LeafObj> {
public:
    using WiVeBVH8<LeafObj>::WiVeBVH8;
    //WiVeBVH8Build8() = default;
    WiVeBVH8Build8(WiVeBVH8Build8<LeafObj>&& other)
        : WiVeBVH8<LeafObj>(std::move(other))
    {
    }

    WiVeBVH8Build8<LeafObj>& operator=(WiVeBVH8Build8<LeafObj>&& other)
    {
        this->m_leafObjects = std::move(other.m_leafObjects);
        this->m_primitives = std::move(other.m_primitives);
        this->m_innerNodeAllocator = std::move(other.m_innerNodeAllocator);
        this->m_leafNodeAllocator = std::move(other.m_leafNodeAllocator);
        this->m_compressedRootHandle = other.m_compressedRootHandle;

        other.m_compressedRootHandle = 0;
        return *this;
    }

protected:
    void commit(gsl::span<RTCBuildPrimitive> embreePrims, gsl::span<LeafObj> objects) override final;

private:
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);
};

}

#include "wive_bvh8_build8_impl.h"
