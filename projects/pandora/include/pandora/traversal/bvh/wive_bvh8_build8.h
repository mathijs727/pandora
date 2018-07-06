#pragma once
#include "wive_bvh8.h"
#include <variant>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8Build8 : public WiVeBVH8<LeafObj> {
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
