// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_OOCBATCHING_PANDORA_SERIALIZATION_H_
#define FLATBUFFERS_GENERATED_OOCBATCHING_PANDORA_SERIALIZATION_H_

#include "flatbuffers/flatbuffers.h"

#include "contiguous_allocator_generated.h"
#include "data_types_generated.h"
#include "scene_generated.h"
#include "triangle_mesh_generated.h"
#include "wive_bvh8_generated.h"

namespace pandora {
namespace serialization {

struct OOCBatchingTopLevelLeafNode;

struct OOCBatchingTopLevelLeafNode FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_UNIQUE_GEOMETRY = 4,
    VT_INSTANCE_BASE_GEOMETRY = 6,
    VT_INSTANCE_BASE_BVH = 8,
    VT_INSTANCED_IDS = 10,
    VT_INSTANCED_GEOMETRY = 12,
    VT_BVH = 14
  };
  const flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *unique_geometry() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *>(VT_UNIQUE_GEOMETRY);
  }
  flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *mutable_unique_geometry() {
    return GetPointer<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *>(VT_UNIQUE_GEOMETRY);
  }
  const flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *instance_base_geometry() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *>(VT_INSTANCE_BASE_GEOMETRY);
  }
  flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *mutable_instance_base_geometry() {
    return GetPointer<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *>(VT_INSTANCE_BASE_GEOMETRY);
  }
  const flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>> *instance_base_bvh() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>> *>(VT_INSTANCE_BASE_BVH);
  }
  flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>> *mutable_instance_base_bvh() {
    return GetPointer<flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>> *>(VT_INSTANCE_BASE_BVH);
  }
  const flatbuffers::Vector<uint32_t> *instanced_ids() const {
    return GetPointer<const flatbuffers::Vector<uint32_t> *>(VT_INSTANCED_IDS);
  }
  flatbuffers::Vector<uint32_t> *mutable_instanced_ids() {
    return GetPointer<flatbuffers::Vector<uint32_t> *>(VT_INSTANCED_IDS);
  }
  const flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>> *instanced_geometry() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>> *>(VT_INSTANCED_GEOMETRY);
  }
  flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>> *mutable_instanced_geometry() {
    return GetPointer<flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>> *>(VT_INSTANCED_GEOMETRY);
  }
  const WiVeBVH8 *bvh() const {
    return GetPointer<const WiVeBVH8 *>(VT_BVH);
  }
  WiVeBVH8 *mutable_bvh() {
    return GetPointer<WiVeBVH8 *>(VT_BVH);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_UNIQUE_GEOMETRY) &&
           verifier.VerifyVector(unique_geometry()) &&
           verifier.VerifyVectorOfTables(unique_geometry()) &&
           VerifyOffset(verifier, VT_INSTANCE_BASE_GEOMETRY) &&
           verifier.VerifyVector(instance_base_geometry()) &&
           verifier.VerifyVectorOfTables(instance_base_geometry()) &&
           VerifyOffset(verifier, VT_INSTANCE_BASE_BVH) &&
           verifier.VerifyVector(instance_base_bvh()) &&
           verifier.VerifyVectorOfTables(instance_base_bvh()) &&
           VerifyOffset(verifier, VT_INSTANCED_IDS) &&
           verifier.VerifyVector(instanced_ids()) &&
           VerifyOffset(verifier, VT_INSTANCED_GEOMETRY) &&
           verifier.VerifyVector(instanced_geometry()) &&
           verifier.VerifyVectorOfTables(instanced_geometry()) &&
           VerifyOffset(verifier, VT_BVH) &&
           verifier.VerifyTable(bvh()) &&
           verifier.EndTable();
  }
};

struct OOCBatchingTopLevelLeafNodeBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_unique_geometry(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>>> unique_geometry) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_UNIQUE_GEOMETRY, unique_geometry);
  }
  void add_instance_base_geometry(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>>> instance_base_geometry) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_INSTANCE_BASE_GEOMETRY, instance_base_geometry);
  }
  void add_instance_base_bvh(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>>> instance_base_bvh) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_INSTANCE_BASE_BVH, instance_base_bvh);
  }
  void add_instanced_ids(flatbuffers::Offset<flatbuffers::Vector<uint32_t>> instanced_ids) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_INSTANCED_IDS, instanced_ids);
  }
  void add_instanced_geometry(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>>> instanced_geometry) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_INSTANCED_GEOMETRY, instanced_geometry);
  }
  void add_bvh(flatbuffers::Offset<WiVeBVH8> bvh) {
    fbb_.AddOffset(OOCBatchingTopLevelLeafNode::VT_BVH, bvh);
  }
  explicit OOCBatchingTopLevelLeafNodeBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  OOCBatchingTopLevelLeafNodeBuilder &operator=(const OOCBatchingTopLevelLeafNodeBuilder &);
  flatbuffers::Offset<OOCBatchingTopLevelLeafNode> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<OOCBatchingTopLevelLeafNode>(end);
    return o;
  }
};

inline flatbuffers::Offset<OOCBatchingTopLevelLeafNode> CreateOOCBatchingTopLevelLeafNode(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>>> unique_geometry = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<GeometricSceneObjectGeometry>>> instance_base_geometry = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<WiVeBVH8>>> instance_base_bvh = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint32_t>> instanced_ids = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<InstancedSceneObjectGeometry>>> instanced_geometry = 0,
    flatbuffers::Offset<WiVeBVH8> bvh = 0) {
  OOCBatchingTopLevelLeafNodeBuilder builder_(_fbb);
  builder_.add_bvh(bvh);
  builder_.add_instanced_geometry(instanced_geometry);
  builder_.add_instanced_ids(instanced_ids);
  builder_.add_instance_base_bvh(instance_base_bvh);
  builder_.add_instance_base_geometry(instance_base_geometry);
  builder_.add_unique_geometry(unique_geometry);
  return builder_.Finish();
}

inline flatbuffers::Offset<OOCBatchingTopLevelLeafNode> CreateOOCBatchingTopLevelLeafNodeDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *unique_geometry = nullptr,
    const std::vector<flatbuffers::Offset<GeometricSceneObjectGeometry>> *instance_base_geometry = nullptr,
    const std::vector<flatbuffers::Offset<WiVeBVH8>> *instance_base_bvh = nullptr,
    const std::vector<uint32_t> *instanced_ids = nullptr,
    const std::vector<flatbuffers::Offset<InstancedSceneObjectGeometry>> *instanced_geometry = nullptr,
    flatbuffers::Offset<WiVeBVH8> bvh = 0) {
  return pandora::serialization::CreateOOCBatchingTopLevelLeafNode(
      _fbb,
      unique_geometry ? _fbb.CreateVector<flatbuffers::Offset<GeometricSceneObjectGeometry>>(*unique_geometry) : 0,
      instance_base_geometry ? _fbb.CreateVector<flatbuffers::Offset<GeometricSceneObjectGeometry>>(*instance_base_geometry) : 0,
      instance_base_bvh ? _fbb.CreateVector<flatbuffers::Offset<WiVeBVH8>>(*instance_base_bvh) : 0,
      instanced_ids ? _fbb.CreateVector<uint32_t>(*instanced_ids) : 0,
      instanced_geometry ? _fbb.CreateVector<flatbuffers::Offset<InstancedSceneObjectGeometry>>(*instanced_geometry) : 0,
      bvh);
}

inline const pandora::serialization::OOCBatchingTopLevelLeafNode *GetOOCBatchingTopLevelLeafNode(const void *buf) {
  return flatbuffers::GetRoot<pandora::serialization::OOCBatchingTopLevelLeafNode>(buf);
}

inline const pandora::serialization::OOCBatchingTopLevelLeafNode *GetSizePrefixedOOCBatchingTopLevelLeafNode(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<pandora::serialization::OOCBatchingTopLevelLeafNode>(buf);
}

inline OOCBatchingTopLevelLeafNode *GetMutableOOCBatchingTopLevelLeafNode(void *buf) {
  return flatbuffers::GetMutableRoot<OOCBatchingTopLevelLeafNode>(buf);
}

inline bool VerifyOOCBatchingTopLevelLeafNodeBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<pandora::serialization::OOCBatchingTopLevelLeafNode>(nullptr);
}

inline bool VerifySizePrefixedOOCBatchingTopLevelLeafNodeBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<pandora::serialization::OOCBatchingTopLevelLeafNode>(nullptr);
}

inline void FinishOOCBatchingTopLevelLeafNodeBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<pandora::serialization::OOCBatchingTopLevelLeafNode> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedOOCBatchingTopLevelLeafNodeBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<pandora::serialization::OOCBatchingTopLevelLeafNode> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace serialization
}  // namespace pandora

#endif  // FLATBUFFERS_GENERATED_OOCBATCHING_PANDORA_SERIALIZATION_H_
