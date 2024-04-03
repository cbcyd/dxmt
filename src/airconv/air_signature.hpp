#pragma once

#include "adt.hpp"
#include "air_type.hpp"
#include "shader_common.hpp"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <string>
#include <unordered_set>
#include <variant>

namespace dxmt::air {

// static llvm::StructType *
// getOrCreateStructType(llvm::StringRef Name, llvm::ArrayRef<llvm::Type *>
// EltTys,
//                       llvm::LLVMContext &Ctx) {
//   using namespace llvm;
//   StructType *ST = StructType::getTypeByName(Ctx, Name);
//   if (ST)
//     return ST;

//   return StructType::create(Ctx, EltTys, Name);
// }

static llvm::StructType *
getOrCreateStructType(llvm::StringRef Name, llvm::LLVMContext &Ctx) {
  using namespace llvm;
  StructType *ST = StructType::getTypeByName(Ctx, Name);
  if (ST)
    return ST;

  return StructType::create(Ctx, Name);
}

constexpr auto get_name = [](auto msl_type) {
  return std::visit(
    [](auto msl_type) { return msl_type.get_name(); }, msl_type
  );
};

constexpr auto get_llvm_type = [](auto msl_type, llvm::LLVMContext &context) {
  return std::visit([&](auto x) { return x.get_llvm_type(context); }, msl_type);
};

enum class MemoryAccess : uint32_t { sample = 0, read, write, read_write };

enum class AddressSpace : uint32_t { unknown, device, constant, threadgroup };

enum class Sign { inapplicable, with_sign, no_sign };

// I use kind to not confuse with type which often refers to component type
enum class TextureKind {
  texture_1d,
  texture_1d_array,
  texture_2d,
  texture_2d_array,
  texture_2d_ms,
  texture_2d_ms_array,
  texture_3d,
  texture_cube,
  texture_cube_array,
  texture_buffer, // 10
  depth_2d,
  depth_2d_array,
  depth_2d_ms,
  depth_2d_ms_array,
  depth_cube,
  depth_cube_array // 16
};

enum class Interpolation {
  center_perspective,
  center_no_perspective,
  centroid_perspective,
  centroid_no_perspective,
  sample_perspective,
  sample_no_perspective,
  flat
};

struct MSLFloat {
  std::string get_name() const { return "float"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    return llvm::Type::getFloatTy(context);
  };
};

struct MSLInt {
  std::string get_name() const { return "int"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    return llvm::Type::getInt32Ty(context);
  };
};

struct MSLUint {
  std::string get_name() const { return "uint"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    return llvm::Type::getInt32Ty(context);
  };
};

struct MSLBool {
  std::string get_name() const { return "bool"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    return llvm::Type::getInt1Ty(context);
  };
};

struct MSLSampler {
  std::string get_name() const { return "sampler"; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    return getOrCreateStructType("struct._sampler_t", context)
      ->getPointerTo(2); // samplers are in constant addrspace
  };
};

constexpr auto msl_bool = MSLBool{};
constexpr auto msl_int = MSLInt{};
constexpr auto msl_uint = MSLUint{};
constexpr auto msl_float = MSLFloat{};

using MSLScalerType =
  std::variant<MSLFloat, MSLInt, MSLUint, MSLBool>; // incomplete list

constexpr auto msl_sampler = MSLSampler{};

struct MSLVector {
  uint32_t dimension;
  MSLScalerType scaler;
  std::string get_name() const {
    auto scaler_name =
      std::visit([](auto scaler) { return scaler.get_name(); }, scaler);
    if (dimension == 1)
      return scaler_name;
    return scaler_name + std::to_string(dimension);
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    auto scaler_type =
      std::visit([&](auto x) { return x.get_llvm_type(context); }, scaler);
    if (dimension == 1) {
      return scaler_type;
    }
    return llvm::FixedVectorType::get(scaler_type, dimension);
  };
};

constexpr auto msl_int2 = MSLVector{2, msl_int};
constexpr auto msl_uint2 = MSLVector{2, msl_uint};
constexpr auto msl_float2 = MSLVector{2, msl_float};
constexpr auto msl_int3 = MSLVector{3, msl_int};
constexpr auto msl_uint3 = MSLVector{3, msl_uint};
constexpr auto msl_float3 = MSLVector{3, msl_float};
constexpr auto msl_int4 = MSLVector{4, msl_int};
constexpr auto msl_uint4 = MSLVector{4, msl_uint};
constexpr auto msl_float4 = MSLVector{4, msl_float};

struct MSLTexture {
  MSLScalerType component_type;
  MemoryAccess memory_access;
  TextureKind resource_kind;

  std::string get_name() const {
    auto component =
      std::visit([](auto scaler) { return scaler.get_name(); }, component_type);
    auto access = [](auto access) -> std::string {
      switch (access) {
      case MemoryAccess::read:
        return "read";
      case MemoryAccess::read_write:
        return "read_write";
      case MemoryAccess::write:
        return "write";
      case MemoryAccess::sample:
        return "sample";
      }
      assert(0 && "unhandled memory access");
    }(memory_access);

    switch (resource_kind) {
    case TextureKind::texture_1d:
      return "texture1d<" + component + "," + access + ">";
    case TextureKind::texture_1d_array:
      return "texture1d_array<" + component + "," + access + ">";
    case TextureKind::texture_2d:
      return "texture2d<" + component + "," + access + ">";
    case TextureKind::texture_2d_array:
      return "texture2d_array<" + component + "," + access + ">";
    case TextureKind::texture_2d_ms:
      return "texture2d_ms<" + component + "," + access + ">";
    case TextureKind::texture_2d_ms_array:
      return "texture2d_ms_array<" + component + "," + access + ">";
    case TextureKind::texture_3d:
      return "texture3d<" + component + "," + access + ">";
    case TextureKind::texture_buffer:
      return "texture_buffer<" + component + "," + access + ">";
    case TextureKind::texture_cube:
      return "texturecube<" + component + "," + access + ">";
    case TextureKind::texture_cube_array:
      return "texturecube_array<" + component + "," + access + ">";
    case TextureKind::depth_2d:
      return "depth2d<" + component + "," + access + ">";
    case TextureKind::depth_2d_array:
      return "depth2d_array<" + component + "," + access + ">";
    case TextureKind::depth_2d_ms:
      return "depth2d_ms<" + component + "," + access + ">";
    case TextureKind::depth_2d_ms_array:
      return "depth2d_ms_array<" + component + "," + access + ">";
    case TextureKind::depth_cube:
      return "depthcube<" + component + "," + access + ">";
    case TextureKind::depth_cube_array:
      return "depthcube_array<" + component + "," + access + ">";
    default:
      assert(0 && "unhandled resource kind");
    }
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) const {
    switch (resource_kind) {
    case TextureKind::texture_1d:
      return getOrCreateStructType("struct._texture_1d_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_1d_array:
      return getOrCreateStructType("struct._texture_1d_array_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_2d:
      return getOrCreateStructType("struct._texture_2d_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_2d_array:
      return getOrCreateStructType("struct._texture_2d_array_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_2d_ms:
      return getOrCreateStructType("struct._texture_2d_ms_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_2d_ms_array:
      return getOrCreateStructType("struct._texture_2d_ms_array_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_3d:
      return getOrCreateStructType("struct._texture_3d_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_cube:
      return getOrCreateStructType("struct._texture_cube_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_cube_array:
      return getOrCreateStructType("struct._texture_cube_array_t", context)
        ->getPointerTo(1);
    case TextureKind::texture_buffer:
      return getOrCreateStructType("struct._texture_buffer_1d_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_2d:
      return getOrCreateStructType("struct._depth_2d_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_2d_array:
      return getOrCreateStructType("struct._depth_2d_array_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_2d_ms:
      return getOrCreateStructType("struct._depth_2d_ms_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_2d_ms_array:
      return getOrCreateStructType("struct._depth_2d_ms_array_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_cube:
      return getOrCreateStructType("struct._depth_cube_t", context)
        ->getPointerTo(1);
    case TextureKind::depth_cube_array:
      return getOrCreateStructType("struct._depth_cube_array_t", context)
        ->getPointerTo(1);
      break;
    };
  };

  struct TextureOperationInfo {
    bool allow_sample_offset = false;
    bool sample_unknown_bit = false;
    bool allow_sample_gradient = false;
    bool allow_gather = false;
    bool allow_read = false;
    bool allow_write = false;
    bool is_depth = false;
    bool is_array = false;
    Sign sign = Sign::inapplicable;
    std::string air_symbol_suffix = {};
    llvm::Type *coord_type = nullptr;
    llvm::Type *offset_type = nullptr;
    llvm::Type *sample_type = nullptr;
    llvm::Type *gather_type = nullptr;
    llvm::Type *read_type = nullptr;
    llvm::Type *write_type = nullptr;
    llvm::Type *gradient_type = nullptr;
  };

  TextureOperationInfo get_operation_info(air::AirType &types) const {
    auto sign = std::visit(
      patterns{
        [](MSLUint) { return Sign::no_sign; },
        [](MSLInt) { return Sign::with_sign; },
        [](auto) { return Sign::inapplicable; }
      },
      component_type
    );
    TextureOperationInfo ret;
    ret.sign = sign;
    switch (resource_kind) {
    case TextureKind::texture_1d: {
      ret.coord_type = types._float;
      ret.allow_sample_offset = true;
      ret.sample_unknown_bit = false;
      ret.offset_type = types._int;
      ret.air_symbol_suffix = "texture_1d";
      ret.sample_type =
        types.to_vec4_type(air::get_llvm_type(component_type, types.context));
      break;
    };
    case TextureKind::texture_2d: {
      ret.coord_type = types._float2;
      ret.allow_sample_offset = true;
      ret.sample_unknown_bit = true;
      ret.offset_type = types._int2;
      ret.air_symbol_suffix = "texture_2d";
      ret.sample_type =
        types.to_vec4_type(air::get_llvm_type(component_type, types.context));
      break;
    }
    case TextureKind::texture_1d_array:
    case TextureKind::texture_2d_array:
    case TextureKind::texture_3d:
    case TextureKind::texture_cube:
    case TextureKind::texture_cube_array:
    case TextureKind::texture_buffer:
    case TextureKind::depth_2d:
    case TextureKind::depth_2d_array:
    case TextureKind::depth_cube:
    case TextureKind::depth_cube_array:
    case TextureKind::texture_2d_ms:
    case TextureKind::texture_2d_ms_array:
    case TextureKind::depth_2d_ms:
    case TextureKind::depth_2d_ms_array: {
      assert(0 && "TODO");
    }
    }
    return ret;
  }
};

// I mean does metal really care about it?
struct MSLWhateverStruct {
  std::string name;
  llvm::Type *type;
  std::string get_name() { return name; };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) { return type; };
};

using MSLScalerOrVectorType =
  template_concat_t<MSLScalerType, std::variant<MSLVector>>;

using MSLRepresentableType = template_concat_t<
  MSLScalerOrVectorType,
  std::variant<MSLSampler, MSLTexture, MSLWhateverStruct>>;

struct MSLPointer {
  MSLRepresentableType pointee;
  AddressSpace address_space;
  std::string get_name() {
    // fking weird
    return std::visit([](auto x) { return x.get_name(); }, pointee);
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    auto pointee_type =
      std::visit([&](auto x) { return x.get_llvm_type(context); }, pointee);
    return pointee_type->getPointerTo((uint32_t)address_space);
  };
};

struct MSLStaticArray {
  uint32_t array_size;
  MSLRepresentableType element_type;
  std::string get_name() {
    return "array<" +
           std::visit(
             [](auto scaler) { return scaler.get_name(); }, element_type
           ) +
           "," + std::to_string(array_size) + ">";
  };

  llvm::Type *get_llvm_type(llvm::LLVMContext &context) {
    return llvm::ArrayType::get(
      std::visit(
        [&](auto x) { return x.get_llvm_type(context); }, element_type
      ),
      array_size
    );
  }
};

using MSLRepresentableTypeWithArray = template_concat_t<
  std::variant<MSLStaticArray, MSLPointer /* why are you here? */>>;

struct ArgumentBindingBuffer {
  std::optional<uint32_t> buffer_size; // unbounded if not represented
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  AddressSpace address_space;
  // std::string arg_type_name;
  MSLRepresentableType type;
  std::string arg_name;
};

struct ArgumentBindingSampler {
  uint32_t location_index;
  uint32_t array_size;
  std::string arg_name;
};

struct ArgumentBindingTexture {
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  MSLTexture type; // why it's a variant!
  std::string arg_name;
};

/* is this in fact argument buffer? */
struct ArgumentBindingIndirectBuffer {
  uint32_t location_index;
  uint32_t array_size;
  MemoryAccess memory_access;
  AddressSpace address_space;
  llvm::StructType *struct_type;
  llvm::Metadata *struct_type_info;
  std::string arg_name;
};

struct ArgumentBindingIndirectConstant {
  uint32_t location_index;
  uint32_t array_size;
  MSLRepresentableTypeWithArray type;
  std::string arg_name;
};

using ArgumentBufferArguments = std::variant<
  ArgumentBindingBuffer, ArgumentBindingIndirectBuffer, ArgumentBindingTexture,
  ArgumentBindingIndirectConstant, ArgumentBindingSampler>;

using FunctionArguments = std::variant<
  ArgumentBindingBuffer, ArgumentBindingIndirectBuffer, ArgumentBindingTexture,
  ArgumentBindingSampler>;

class ArgumentBufferBuilder {
  /* the return value is the element index of structure (as well as in argument
   * buffer) */
public:
  uint32_t DefineBuffer(
    std::string name, AddressSpace addressp_space, MemoryAccess access,
    MSLRepresentableType type, uint32_t location_index = UINT32_MAX
  );
  // uint32_t DefineIndirectBuffer(
  //   std::string name, llvm::StructType* struct_type, llvm::Metadata*
  //   struct_type_metadata
  // );
  uint32_t DefineTexture(
    std::string name, TextureKind kind, MemoryAccess access,
    MSLScalerType scaler_type, uint32_t location_index = UINT32_MAX
  );
  uint32_t
  DefineSampler(std::string name, uint32_t location_index = UINT32_MAX);
  uint32_t
  DefineInteger32(std::string name, uint32_t location_index = UINT32_MAX);
  uint32_t
  DefineFloat32(std::string name, uint32_t location_index = UINT32_MAX);

  auto Build(llvm::LLVMContext &context, llvm::Module &module)
    -> std::tuple<llvm::StructType *, llvm::MDNode *>;

  auto empty() { return fieldsType.empty(); }

private:
  std::vector<ArgumentBufferArguments> fieldsType;
  std::unordered_set<std::string> fields;
};

struct InputVertexStageIn {
  uint32_t attribute;
  MSLScalerOrVectorType type;
  std::string name;
};

struct OutputVertex {
  std::string user;
  MSLScalerOrVectorType type;
};

struct OutputPosition {
  MSLScalerOrVectorType type;
};

struct InputFragmentStageIn {
  std::string user;
  MSLScalerOrVectorType type;
  Interpolation interpolation;
};

struct InputVertexID {};
struct InputInstanceID {};
struct InputViewportArrayIndex {};
struct InputRenderTargetArrayIndex {};

struct InputPrimitiveID {};
struct InputFrontFacing {};
struct InputInputCoverage {};
struct InputSampleIndex {};

struct InputPosition {
  Interpolation interpolation;
};

struct InputThreadIndexInThreadgroup {};    // uint
struct InputThreadPositionInThreadgroup {}; // uint3
struct InputThreadPositionInGrid {};        // uint3
struct InputThreadgroupPositionInGrid {};   // uint3

struct OutputRenderTarget {
  uint32_t index;
  MSLScalerOrVectorType type;
};

struct OutputDepth {};

using FunctionInput = template_concat_t<
  FunctionArguments,
  std::variant<
    /* vertex */
    InputVertexID, InputVertexStageIn, InputInstanceID,
    /* fragment */
    InputPrimitiveID, InputViewportArrayIndex, InputRenderTargetArrayIndex,
    InputFrontFacing, InputPosition, InputFragmentStageIn,
    /* kernel */
    InputThreadIndexInThreadgroup, InputThreadPositionInThreadgroup,
    InputThreadPositionInGrid, InputThreadgroupPositionInGrid>>;

using FunctionOutput = std::variant<
  /* vertex */
  OutputVertex, OutputPosition,
  /* fragment */
  OutputRenderTarget>;

class FunctionSignatureBuilder {
public:
  uint32_t DefineInput(const FunctionInput &input);
  uint32_t DefineOutput(const FunctionOutput &output);

  auto CreateFunction(
    std::string name, llvm::LLVMContext &context, llvm::Module &module
  ) -> std::pair<llvm::Function *, llvm::MDNode *>;

private:
  std::vector<FunctionInput> inputs;
  std::vector<FunctionOutput> outputs;
};

inline TextureKind to_air_resource_type(
  dxmt::shader::common::ResourceType type, bool use_depth = false
) {
  switch (type) {
  case shader::common::ResourceType::TextureBuffer:
    return TextureKind::texture_buffer;
  case shader::common::ResourceType::Texture1D:
    return TextureKind::texture_1d;
  case shader::common::ResourceType::Texture1DArray:
    return TextureKind::texture_1d_array;
  case shader::common::ResourceType::Texture2D:
    return use_depth ? TextureKind::depth_2d : TextureKind::texture_2d;
  case shader::common::ResourceType::Texture2DArray:
    return use_depth ? TextureKind::depth_2d_array
                     : TextureKind::texture_2d_array;
  case shader::common::ResourceType::Texture2DMultisampled:
    return use_depth ? TextureKind::depth_2d_ms : TextureKind::texture_2d_ms;
  case shader::common::ResourceType::Texture2DMultisampledArray:
    return use_depth ? TextureKind::depth_2d_ms_array
                     : TextureKind::texture_2d_ms_array;
  case shader::common::ResourceType::Texture3D:
    return TextureKind::texture_3d;
  case shader::common::ResourceType::TextureCube:
    return use_depth ? TextureKind::depth_cube : TextureKind::texture_cube;
  case shader::common::ResourceType::TextureCubeArray:
    return use_depth ? TextureKind::depth_cube_array
                     : TextureKind::texture_cube_array;
  };
};

inline MSLScalerType
to_air_scaler_type(dxmt::shader::common::ScalerDataType type) {
  switch (type) {
  case shader::common::ScalerDataType::Float:
    return msl_float;
  case shader::common::ScalerDataType::Uint:
    return msl_uint;
  case shader::common::ScalerDataType::Int:
    return msl_int;
  case shader::common::ScalerDataType::Double:
    assert(0 && "");
    break;
  }
}

} // namespace dxmt::air