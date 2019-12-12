#include "pandora/traversal/offline_bvh_cache.h"
#include "pandora/graphics_core/shape.h"

namespace pandora {

Bounds Leaf::getBounds() const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        auto prim = std::get<Primitive>(m_object);
        return prim.pShape->getPrimitiveBounds(prim.primID);
    } else {
        auto pCachedBVHScene = std::get<CachedBVHScene*>(m_object);
        return pCachedBVHScene->getBounds();
    }
}

bool Leaf::intersect(Ray& ray, RayHit& rayHit) const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        auto prim = std::get<Primitive>(m_object);
        return prim.pShape->intersectPrimitive(ray, rayHit, prim.primID);
    } else {
        auto pCachedBVHScene = std::get<CachedBVHScene*>(m_object);
        return pCachedBVHScene->intersect(ray, rayHit);
    }
}

bool Leaf::intersectAny(Ray& ray) const
{
    if (std::holds_alternative<Primitive>(m_object)) {
        auto prim = std::get<Primitive>(m_object);
        RayHit dummyRayHit;
        return prim.pShape->intersectPrimitive(ray, dummyRayHit, prim.primID);
    } else {
        auto pCachedBVHScene = std::get<CachedBVHScene*>(m_object);
        return pCachedBVHScene->intersectAny(ray);
    }
}


CachedBVHScene::CachedBVHScene(WiVeBVH8Build8<Leaf>&& bvh, std::vector<Leaf>&& leafs)
    : Evictable(true)
    , m_bvh(std::move(bvh))
    , m_leafs(std::move(leafs))
{
    m_leafs.shrink_to_fit();

    for (const auto& leaf : m_leafs)
        m_bounds.extend(leaf.getBounds());
}

Bounds CachedBVHScene::getBounds() const
{
    return m_bounds;
}

bool CachedBVHScene::intersect(Ray& ray, RayHit& rayHit) const
{
    return m_bvh->intersect(ray, rayHit);
}

size_t CachedBVHScene::sizeBytes() const
{
    size_t size = m_leafs.capacity() * sizeof(Leaf);
    if (m_bvh.has_value()) {
        size += m_bvh->sizeBytes();
    }
    return size;
}

void CachedBVHScene::serialize(tasking::Serializer& serializer)
{
    //TODO(Mathijs): ...
}

void CachedBVHScene::doEvict()
{
    m_leafs.clear();
    m_leafs.shrink_to_fit();
    m_bvh.reset();
}

void CachedBVHScene::doMakeResident(tasking::Deserializer& deserializer)
{
    //TODO(Mathijs): ...
}

}