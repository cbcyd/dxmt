#include "air_signature.hpp"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>

#include "ftl.hpp"

using namespace llvm;

namespace dxmt::air {

class StreamMDHelper {
public:
  auto string(std::string str) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return MDString::get(context, str);
    });
    return this;
  };
  auto string(const char *c_str) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return MDString::get(context, c_str);
    });
    return this;
  };
  auto integer(uint64_t value, bool is_signed = false) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return ConstantAsMetadata::get(
        ConstantInt::get(context, APInt{32, value, is_signed})
      );
    });
    return this;
  };
  template <typename enumeration>
  auto integer(enumeration value, bool is_signed = false) {
    factories.push_back([=](llvm::LLVMContext &context) {
      return ConstantAsMetadata::get(
        ConstantInt::get(context, APInt{32, (uint32_t)value, is_signed})
      );
    });
    return this;
  };
  auto metadata(llvm::Metadata *metadata) {
    factories.push_back([=](llvm::LLVMContext &context) { return metadata; });
    return this;
  };

  auto BuildTuple(llvm::LLVMContext &context) {
    auto mds = factories | [&](auto factory) -> llvm::Metadata * {
      return factory(context);
    };
    return MDTuple::get(context, mds);
  };

private:
  std::vector<std::function<llvm::Metadata *(llvm::LLVMContext &)>> factories;
};

uint32_t ArgumentBufferBuilder::DefineBuffer(
  std::string name, AddressSpace addressp_space, MemoryAccess access,
  MSLRepresentableType type, uint32_t location_index,
  std::optional<uint32_t> raster_order_group
) {
  auto element_index = fieldsType.size();
  assert(!fields.count(name) && "otherwise duplicated field name");
  fieldsType.push_back(ArgumentBindingBuffer{
    .location_index =
      location_index == UINT32_MAX ? (uint32_t)element_index : location_index,
    .array_size = 1,
    .memory_access = access,
    .address_space = addressp_space,
    .type = type,
    .arg_name = name,
    .raster_order_group = raster_order_group,
  });
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineSampler(
  std::string name, uint32_t location_index
) {
  auto element_index = fieldsType.size();
  assert(!fields.count(name) && "otherwise duplicated field name");
  fieldsType.push_back(ArgumentBindingSampler{
    .location_index =
      location_index == UINT32_MAX ? (uint32_t)element_index : location_index,
    .array_size = 1,
    .arg_name = name
  });
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineTexture(
  std::string name, TextureKind kind, MemoryAccess access,
  MSLScalerType scaler_type, uint32_t location_index,
  std::optional<uint32_t> raster_order_group
) {
  auto element_index = fieldsType.size();
  assert(!fields.count(name) && "otherwise duplicated field name");
  fieldsType.push_back(ArgumentBindingTexture{
    .location_index =
      location_index == UINT32_MAX ? (uint32_t)element_index : location_index,
    .array_size = 1,
    .memory_access = access,
    .type =
      MSLTexture{
        .component_type = scaler_type,
        .memory_access = access,
        .resource_kind = kind
      },
    .arg_name = name,
    .raster_order_group = raster_order_group,
  });
  return element_index;
};

uint32_t ArgumentBufferBuilder::DefineInteger32(
  std::string name, uint32_t location_index
) {
  auto element_index = fieldsType.size();
  assert(!fields.count(name) && "otherwise duplicated field name");
  fieldsType.push_back(ArgumentBindingIndirectConstant{
    .location_index =
      location_index == UINT32_MAX ? (uint32_t)element_index : location_index,
    .array_size = 1,
    .type = msl_uint,
    .arg_name = name
  });
  return element_index;
};

auto build_argument_binding_buffer(
  StreamMDHelper &md, const ArgumentBindingBuffer &buffer,
  llvm::LLVMContext &context, const llvm::DataLayout &layout
) -> llvm::Type * {
  md.string("air.buffer");
  if (buffer.buffer_size.has_value()) {
    md.string("air.buffer_size");
    md.integer(buffer.buffer_size.value());
  }
  md.string("air.location_index");
  md.integer(buffer.location_index);
  md.integer(buffer.array_size);

  switch (buffer.memory_access) {
  case MemoryAccess::read:
    md.string("air.read");
    break;
  case MemoryAccess::write:
    md.string("air.write");
    break;
  case MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case MemoryAccess::sample:
    assert(0 && "buffer can't be sampled");
    break;
  }

  md.string("air.address_space");
  md.integer(buffer.address_space);

  if (buffer.raster_order_group.has_value()) {
    md.string("air.raster_order_group");
    md.integer(buffer.raster_order_group.value());
  }

  auto type = get_llvm_type(buffer.type, context);

  md.string("air.arg_type_size");
  md.integer( // FIXME: bool (i1)
    layout.getTypeSizeInBits(type) / 8
  );

  md.string("air.arg_type_align_size");
  md.integer(layout.getABITypeAlign(type).value());

  md.string("air.arg_type_name");
  md.string(get_name(buffer.type));

  md.string("air.arg_name");
  md.string(buffer.arg_name);

  return type->getPointerTo((uint32_t)buffer.address_space);
};
auto build_argument_binding_indirect_buffer(
  StreamMDHelper &md, const ArgumentBindingIndirectBuffer &indirect_buffer,
  llvm::LLVMContext &context, const llvm::DataLayout &layout
) -> llvm::Type * {
  md.string("air.indirect_buffer");
  auto struct_layout = layout.getStructLayout(indirect_buffer.struct_type);
  md.string("air.buffer_size");
  md.integer((struct_layout->getSizeInBytes() * indirect_buffer.array_size));
  md.string("air.location_index");
  md.integer(indirect_buffer.location_index);
  md.integer(indirect_buffer.array_size);

  switch (indirect_buffer.memory_access) {
  case MemoryAccess::read:
    md.string("air.read");
    break;
  case MemoryAccess::write:
    md.string("air.write");
    break;
  case MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case MemoryAccess::sample:
    assert(0 && "buffer can't be sampled");
    break;
  }

  md.string("air.address_space");
  md.integer(indirect_buffer.address_space);

  md.string("air.struct_type_info");
  md.metadata(indirect_buffer.struct_type_info);

  md.string("air.arg_type_size");
  md.integer(struct_layout->getSizeInBytes());

  md.string("air.arg_type_align_size");
  md.integer(struct_layout->getAlignment().value());

  md.string("air.arg_type_name");
  md.string(indirect_buffer.struct_type->getName().str());

  md.string("air.arg_name");
  md.string(indirect_buffer.arg_name);

  return indirect_buffer.struct_type->getPointerTo((uint32_t
  )indirect_buffer.address_space);
};
auto build_argument_binding_texture(
  StreamMDHelper &md, const ArgumentBindingTexture &texture,
  llvm::LLVMContext &context
) -> llvm::Type * {
  auto texture_type_name = texture.type.get_name();
  md.string("air.texture")
    ->string("air.location_index")
    ->integer(texture.location_index)
    ->integer(texture.array_size);

  switch (texture.memory_access) {
  case dxmt::air::MemoryAccess::read:
    md.string("air.read");
    break;
  case dxmt::air::MemoryAccess::write:
    md.string("air.write");
    break;
  case dxmt::air::MemoryAccess::read_write:
    md.string("air.read_write");
    break;
  case dxmt::air::MemoryAccess::sample:
    md.string("air.sample");
    break;
  }

  if (texture.raster_order_group.has_value()) {
    md.string("air.raster_order_group");
    md.integer(texture.raster_order_group.value());
  }

  md.string("air.arg_type_name")
    ->string(texture_type_name)
    ->string("air.arg_name")
    ->string(texture.arg_name);

  return texture.type.get_llvm_type(context);
};
auto build_argument_binding_sampler(
  StreamMDHelper &md, const ArgumentBindingSampler &sampler,
  llvm::LLVMContext &context
) -> llvm::Type * {
  md.string("air.sampler")
    ->string("air.location_index")
    ->integer(sampler.location_index)
    ->integer(sampler.array_size)
    ->string("air.arg_type_name")
    ->string("sampler")
    ->string("air.arg_name")
    ->string(sampler.arg_name);
  return (MSLSampler{}).get_llvm_type(context);
};
auto insert_inteterpolation(StreamMDHelper &md, Interpolation interpolation) {
  switch (interpolation) {
  case Interpolation::center_perspective:
    md.string("air.center")->string("air.perspective");
    break;
  case Interpolation::center_no_perspective:
    md.string("air.center")->string("air.no_perspective");
    break;
  case Interpolation::centroid_perspective:
    md.string("air.centroid")->string("air.perspective");
    break;
  case Interpolation::centroid_no_perspective:
    md.string("air.centroid")->string("air.no_perspective");
    break;
  case Interpolation::sample_perspective:
    md.string("air.sample")->string("air.perspective");
    break;
  case Interpolation::sample_no_perspective:
    md.string("air.sample")->string("air.no_perspective");
    break;
  case Interpolation::flat:
    md.string("air.flat");
    break;
  }
}

auto ArgumentBufferBuilder::Build(
  llvm::LLVMContext &context, const llvm::DataLayout &layout
) -> std::tuple<llvm::StructType *, llvm::MDNode *> {
  std::vector<llvm::Type *> fields;
  std::vector<llvm::Metadata *> indirect_argument;
  uint32_t offset = 0;
  for (auto argument : fieldsType) {
    StreamMDHelper metadata_field;
    metadata_field.integer(offset);
    auto field_type = std::visit(
      patterns{
        [&](const ArgumentBindingBuffer &buffer) {
          return build_argument_binding_buffer(
            metadata_field, buffer, context, layout
          );
        },
        [&](const ArgumentBindingIndirectBuffer &indirect_buffer) {
          return build_argument_binding_indirect_buffer(
            metadata_field, indirect_buffer, context, layout
          );
        },
        [&](const ArgumentBindingTexture &texture) {
          return build_argument_binding_texture(
            metadata_field, texture, context
          );
        },
        [&](const ArgumentBindingSampler &sampler) {
          return build_argument_binding_sampler(
            metadata_field, sampler, context
          );
        },
        [&](const ArgumentBindingIndirectConstant &constant) {
          metadata_field.string("air.indirect_constant")
            ->string("air.location_index")
            ->integer(constant.location_index)
            ->integer(constant.array_size)
            ->string("air.arg_type_name")
            ->string(get_name(constant.type))
            ->string("air.arg_name")
            ->string(constant.arg_name);
          return get_llvm_type(constant.type, context);
        }
      },
      argument
    );
    fields.push_back(field_type);
    indirect_argument.push_back(metadata_field.BuildTuple(context));
    offset++;
  };

  auto struct_type =
    StructType::create(context, fields, "argument_buffer_struct");

  // struct_type.
  auto struct_layout = layout.getStructLayout(struct_type);
  auto member_offsets = struct_layout->getMemberOffsets();
  StreamMDHelper struct_metadata;
  for (auto [field_type, member_offset, indirect_argument_metadata] :
       zip(fields, member_offsets, indirect_argument)) {
    struct_metadata.integer(member_offset)
      ->integer(
        layout.getTypeSizeInBits(field_type) / 8
      )            // CAVEATS: the bool is represented as i8 in arugment
                   // buffer (but i1 in function argument)
      ->integer(0) // TODO: array_size (0 for non-array, like
                   // MTLArguementDescriptor)
      ->string(cast<MDString>(
                 cast<MDTuple>(indirect_argument_metadata)->operands().end()[-3]
      )
                 ->getString()
                 .str()) // FIXME: is this guaranteed?
      ->string(cast<MDString>(
                 cast<MDTuple>(indirect_argument_metadata)->operands().end()[-1]
      )
                 ->getString()
                 .str()) // FIXME: is this guaranteed?
      ->string("air.indirect_argument")
      ->metadata(indirect_argument_metadata);
  }
  return std::make_pair(struct_type, struct_metadata.BuildTuple(context));
};

uint32_t FunctionSignatureBuilder::DefineInput(const FunctionInput &input) {
  uint32_t index = inputs.size();
  for (auto &element : enumerate(inputs)) {
    if (element.value().index() == input.index()) {
      if (std::visit(
            patterns{
              [&](const InputVertexStageIn s) {
                return s.attribute ==
                       std::get<InputVertexStageIn>(element.value()).attribute;
              },
              [&](const InputFragmentStageIn s) {
                return s.user ==
                       std::get<InputFragmentStageIn>(element.value()).user;
              },
              [&](const ArgumentBindingBuffer s) { return false; },
              [&](const ArgumentBindingSampler s) { return false; },
              [&](const ArgumentBindingTexture s) { return false; },
              [&](const ArgumentBindingIndirectBuffer s) { return false; },
              [](auto) { return true; }
            },
            input
          )) {
        return element.index();
      }
    }
  }
  inputs.push_back(input);
  return index;
};

uint32_t FunctionSignatureBuilder::DefineOutput(const FunctionOutput &output) {
  // TODO: check duplication (in case it's already defined)
  uint32_t index = outputs.size();
  outputs.push_back(output);
  return index;
};

auto FunctionSignatureBuilder::CreateFunction(
  std::string name, llvm::LLVMContext &context, llvm::Module &module,
  uint64_t sign_mask
) -> std::pair<llvm::Function *, llvm::MDNode *> {
  std::vector<Metadata *> metadata_input;
  std::vector<llvm::Type *> type_input;
  std::vector<Metadata *> metadata_output;
  std::vector<llvm::Type *> type_output;

  auto get_input_attribute_type = [](
                                    InputAttributeComponentType type,
                                    uint32_t attribute, uint64_t sign_mask
                                  ) -> MSLScalerOrVectorType {
    switch (type) {
    case InputAttributeComponentType::Unknown:
    case InputAttributeComponentType::Uint: {
      if (sign_mask & (1 << attribute)) {
        return msl_int4;
      } else {
        return msl_uint4;
      }
    }
    case InputAttributeComponentType::Int: {
      if (sign_mask & (1 << attribute)) {
        return msl_uint4;
      } else {
        return msl_int4;
      }
    }
    case InputAttributeComponentType::Float:
      return msl_float4;
    }
  };
  for (auto &item : enumerate(inputs)) {
    auto i = item.index();
    auto input = item.value();
    StreamMDHelper metadata_field;
    metadata_field.integer(i);
    llvm::Type *field_type = std::visit(
      patterns{
        [&](const InputVertexStageIn &vertex_in) {
          auto type = get_input_attribute_type(
            vertex_in.type, vertex_in.attribute, sign_mask
          );
          metadata_field.string("air.vertex_input")
            ->string("air.location_index")
            ->integer(vertex_in.attribute)
            ->integer(1)
            ->string("air.arg_type_name")
            ->string(get_name(type))
            ->string("air.arg_name")
            ->string(vertex_in.name);
          return get_llvm_type(type, context);
        },
        [&](const InputFragmentStageIn &frag_in) {
          metadata_field.string("air.fragment_input")
            ->string(std::string("user(") + frag_in.user + ")");
          insert_inteterpolation(metadata_field, frag_in.interpolation);
          metadata_field.string("air.arg_type_name")
            ->string(get_name(frag_in.type))
            ->string("air.arg_name")
            ->string(frag_in.user);
          return get_llvm_type(frag_in.type, context);
        },
        [&](const ArgumentBindingBuffer &buffer) {
          return build_argument_binding_buffer(
            metadata_field, buffer, context, module.getDataLayout()
          );
        },
        [&](const ArgumentBindingIndirectBuffer &indirect_buffer) {
          return build_argument_binding_indirect_buffer(
            metadata_field, indirect_buffer, context, module.getDataLayout()
          );
        },
        [&](const ArgumentBindingTexture &texture) {
          return build_argument_binding_texture(
            metadata_field, texture, context
          );
        },
        [&](const ArgumentBindingSampler &sampler) {
          return build_argument_binding_sampler(
            metadata_field, sampler, context
          );
        },
        [&](const InputPosition &position) {
          metadata_field.string("air.position");
          insert_inteterpolation(metadata_field, position.interpolation);
          metadata_field.string("air.arg_type_name")
            ->string("float4") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_position");

          return msl_float4.get_llvm_type(context);
        },
        [&](const InputFrontFacing &) {
          metadata_field.string("air.front_facing")
            ->string("air.arg_type_name")
            ->string("bool") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_frontface");
          return msl_bool.get_llvm_type(context);
        },
        [&](const InputSampleIndex &) {
          metadata_field.string("air.sample_id")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_sample_index");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputRenderTargetArrayIndex &) {
          metadata_field.string("air.render_target_array_index")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_rt_index");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputViewportArrayIndex &) {
          metadata_field.string("air.viewport_array_index")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_vp_index");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputThreadgroupPositionInGrid &) {
          metadata_field.string("air.threadgroup_position_in_grid")
            ->string("air.arg_type_name")
            ->string("uint3") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_threadgroup_position_in_grid");
          return msl_uint3.get_llvm_type(context);
        },
        [&](const InputThreadPositionInGrid &) {
          metadata_field.string("air.thread_position_in_grid")
            ->string("air.arg_type_name")
            ->string("uint3") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_thread_position_in_grid");
          return msl_uint3.get_llvm_type(context);
        },
        [&](const InputThreadPositionInThreadgroup &) {
          metadata_field.string("air.thread_position_in_threadgroup")
            ->string("air.arg_type_name")
            ->string("uint3") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_thread_position_in_threadgroup");
          return msl_uint3.get_llvm_type(context);
        },
        [&](const InputThreadIndexInThreadgroup &) {
          metadata_field.string("air.thread_index_in_threadgroup")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_thread_index_in_threadgroup");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputVertexID &) {
          metadata_field.string("air.vertex_id")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_vertex_id");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputBaseVertex &) {
          metadata_field.string("air.base_vertex")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_base_vertex");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputInstanceID &) {
          metadata_field.string("air.instance_id")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_instance_id");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputBaseInstance &) {
          metadata_field.string("air.base_instance")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_base_instance");
          return msl_uint.get_llvm_type(context);
        },
        [&](const InputPrimitiveID &) {
          metadata_field.string("air.primitive_id")
            ->string("air.arg_type_name")
            ->string("uint") // HARDCODED
            ->string("air.arg_name")
            ->string("mtl_primitive_id");
          return msl_uint.get_llvm_type(context);
        },
        [](auto _) {
          assert(0 && "Unhandled input");
          return (llvm::Type *)nullptr;
        }
      },
      input
    );

    metadata_input.push_back(metadata_field.BuildTuple(context));
    type_input.push_back(field_type);
  };
  for (auto &item : enumerate(outputs)) {
    auto output = item.value();
    StreamMDHelper md;
    llvm::Type *field_type = std::visit(
      patterns{
        [&](const OutputVertex &vertex_out) {
          md.string("air.vertex_output")
            ->string("user(" + vertex_out.user + ")")
            ->string("air.arg_type_name")
            ->string(get_name(vertex_out.type))
            ->string("air.arg_name")
            ->string(vertex_out.user);
          return get_llvm_type(vertex_out.type, context);
        },
        [&](const OutputRenderTarget &render_target) {
          md.string("air.render_target")
            ->integer(render_target.index)
            ->integer(0) // TODO: dual source blending
            ->string("air.arg_type_name")
            ->string(get_name(render_target.type))
            ->string("air.arg_name")
            ->string("render_target_" + std::to_string(render_target.index));
          return get_llvm_type(render_target.type, context);
        },
        [&](const OutputPosition &position) {
          md.string("air.position")
            ->string("air.arg_type_name")
            ->string(get_name(position.type))
            ->string("air.arg_name")
            ->string("mtl_position");
          return get_llvm_type(position.type, context);
        },
        [&](const OutputDepth &depth) {
          static const char *qualifiers[3] = {
            "air.any", "air.greater", "air.less"
          };
          md.string("air.depth")
            ->string("air.depth_qualifier")
            ->string(qualifiers[(uint32_t)depth.depth_argument])
            ->string("air.arg_type_name")
            ->string("float")
            ->string("air.arg_name")
            ->string("mtl_depth");
          return Type::getFloatTy(context);
        },
        [](auto _) {
          assert(0 && "Unhandled output");
          return (llvm::Type *)nullptr;
        }
      },
      output
    );
    metadata_output.push_back(md.BuildTuple(context));
    type_output.push_back(field_type);
  };
  auto output_struct_type = type_output.size() > 0
                              ? StructType::get(context, type_output, true)
                              : Type::getVoidTy(context);
  auto function = Function::Create(
    FunctionType::get(output_struct_type, type_input, false),
    llvm::Function::LinkageTypes::ExternalLinkage, name, module
  );
  auto metadata_output_tuple = MDTuple::get(context, metadata_output);
  auto metadata_input_tuple = MDTuple::get(context, metadata_input);
  std::vector<Metadata *> function_def_tuple = {
    ConstantAsMetadata::get(function), metadata_output_tuple,
    metadata_input_tuple
  };
  if (early_fragment_tests) {
    function_def_tuple.push_back(MDString::get(context, "early_fragment_tests")
    );
  }
  if (max_work_group_size > 0) {
    auto tuple = MDTuple::get(
      context, {MDString::get(context, "air.max_work_group_size"),
                ConstantAsMetadata::get(
                  ConstantInt::get(context, APInt{32, max_work_group_size})
                )}
    );
    function_def_tuple.push_back(tuple);
    function->addAttributeAtIndex(
      ~0U, Attribute::get(
             context, "max-work-group-size", std::to_string(max_work_group_size)
           )
    );
  }
  return std::make_pair(function, MDTuple::get(context, function_def_tuple));
};

} // namespace dxmt::air