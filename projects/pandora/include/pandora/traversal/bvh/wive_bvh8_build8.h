#pragma once
#include "wive_bvh8.h"
#include <variant>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8Build8 : public WiVeBVH8<LeafObj> {
public:
    using WiVeBVH8<LeafObj>::WiVeBVH8;
    WiVeBVH8Build8(WiVeBVH8Build8<LeafObj>&& other)
    {
		m_leafObjects = std::move(other.m_leafObjects);
		m_primitives = std::move(other.m_primitives);
		m_innerNodeAllocator = std::move(other.m_innerNodeAllocator);
		m_leafNodeAllocator = std::move(other.m_leafNodeAllocator);
		m_compressedRootHandle = other.m_compressedRootHandle;
		other.m_leafObjects.clear();
		other.m_primitives.clear();
		other.m_innerNodeAllocator = nullptr;
		other.m_leafNodeAllocator = nullptr;
		other.m_compressedRootHandle = 0;
    }

    WiVeBVH8Build8<LeafObj>& operator=(WiVeBVH8Build8<LeafObj>&& other)
    {
        m_leafObjects = std::move(other.m_leafObjects);
        m_primitives = std::move(other.m_primitives);
        m_innerNodeAllocator = std::move(other.m_innerNodeAllocator);
        m_leafNodeAllocator = std::move(other.m_leafNodeAllocator);
        m_compressedRootHandle = other.m_compressedRootHandle;
		other.m_leafObjects.clear();
		other.m_primitives.clear();
		other.m_innerNodeAllocator = nullptr;
		other.m_leafNodeAllocator = nullptr;
		other.m_compressedRootHandle = 0;
        return *this;
    }

protected:
    void commit() override final;

private:
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);
};

}

#include "wive_bvh8_build8_impl.h"
