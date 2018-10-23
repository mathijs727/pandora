// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SCENE_PANDORA_SERIALIZATION_H_
#define FLATBUFFERS_GENERATED_SCENE_PANDORA_SERIALIZATION_H_

#include "flatbuffers/flatbuffers.h"

#include "data_types_generated.h"
#include "triangle_mesh_generated.h"

namespace pandora {
namespace serialization {

struct InstancedSceneObjectGeometry;

struct GeometricSceneObjectGeometry;

struct InstancedSceneObjectGeometry FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_TRANSFORM = 4
  };
  const Transform *transform() const {
    return GetStruct<const Transform *>(VT_TRANSFORM);
  }
  Transform *mutable_transform() {
    return GetStruct<Transform *>(VT_TRANSFORM);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<Transform>(verifier, VT_TRANSFORM) &&
           verifier.EndTable();
  }
};

struct InstancedSceneObjectGeometryBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_transform(const Transform *transform) {
    fbb_.AddStruct(InstancedSceneObjectGeometry::VT_TRANSFORM, transform);
  }
  explicit InstancedSceneObjectGeometryBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  InstancedSceneObjectGeometryBuilder &operator=(const InstancedSceneObjectGeometryBuilder &);
  flatbuffers::Offset<InstancedSceneObjectGeometry> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<InstancedSceneObjectGeometry>(end);
    return o;
  }
};

inline flatbuffers::Offset<InstancedSceneObjectGeometry> CreateInstancedSceneObjectGeometry(
    flatbuffers::FlatBufferBuilder &_fbb,
    const Transform *transform = 0) {
  InstancedSceneObjectGeometryBuilder builder_(_fbb);
  builder_.add_transform(transform);
  return builder_.Finish();
}

struct GeometricSceneObjectGeometry FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_GEOMETRY = 4
  };
  const TriangleMesh *geometry() const {
    return GetPointer<const TriangleMesh *>(VT_GEOMETRY);
  }
  TriangleMesh *mutable_geometry() {
    return GetPointer<TriangleMesh *>(VT_GEOMETRY);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_GEOMETRY) &&
           verifier.VerifyTable(geometry()) &&
           verifier.EndTable();
  }
};

struct GeometricSceneObjectGeometryBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_geometry(flatbuffers::Offset<TriangleMesh> geometry) {
    fbb_.AddOffset(GeometricSceneObjectGeometry::VT_GEOMETRY, geometry);
  }
  explicit GeometricSceneObjectGeometryBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  GeometricSceneObjectGeometryBuilder &operator=(const GeometricSceneObjectGeometryBuilder &);
  flatbuffers::Offset<GeometricSceneObjectGeometry> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<GeometricSceneObjectGeometry>(end);
    return o;
  }
};

inline flatbuffers::Offset<GeometricSceneObjectGeometry> CreateGeometricSceneObjectGeometry(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<TriangleMesh> geometry = 0) {
  GeometricSceneObjectGeometryBuilder builder_(_fbb);
  builder_.add_geometry(geometry);
  return builder_.Finish();
}

}  // namespace serialization
}  // namespace pandora

#endif  // FLATBUFFERS_GENERATED_SCENE_PANDORA_SERIALIZATION_H_
