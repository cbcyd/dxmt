#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/VersionTuple.h"

#include "airconv_context.hpp"
#include "airconv_public.h"
#include "dxbc_converter.hpp"
#include "metallib_writer.hpp"

using namespace llvm;

namespace dxmt {

void initializeModule(llvm::Module &M, const ModuleOptions &opts) {
  auto &context = M.getContext();
  M.setSourceFileName("airconv_generated.metal");
  M.setTargetTriple("air64-apple-macosx14.0.0");
  M.setDataLayout(
    "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
    "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
    "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
    "16:32"
  );
  M.setSDKVersion(VersionTuple(14, 0));
  M.addModuleFlag(Module::ModFlagBehavior::Error, "wchar_size", 4);
  M.addModuleFlag(Module::ModFlagBehavior::Max, "frame-pointer", 2);
  M.addModuleFlag(Module::ModFlagBehavior::Max, "air.max_device_buffers", 31);
  M.addModuleFlag(Module::ModFlagBehavior::Max, "air.max_constant_buffers", 31);
  M.addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_threadgroup_buffers", 31
  );
  M.addModuleFlag(Module::ModFlagBehavior::Max, "air.max_textures", 128);
  M.addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_read_write_textures", 8
  );
  M.addModuleFlag(Module::ModFlagBehavior::Max, "air.max_samplers", 16);

  auto createUnsignedInteger = [&](uint32_t s) {
    return ConstantAsMetadata::get(
      ConstantInt::get(context, APInt{32, s, false})
    );
  };
  auto createString = [&](auto s) { return MDString::get(context, s); };

  auto airVersion = M.getOrInsertNamedMetadata("air.version");
  airVersion->addOperand(MDTuple::get(
    context, {createUnsignedInteger(2), createUnsignedInteger(6),
              createUnsignedInteger(0)}
  ));
  auto airLangVersion = M.getOrInsertNamedMetadata("air.language_version");
  airLangVersion->addOperand(MDTuple::get(
    context, {createString("Metal"), createUnsignedInteger(3),
              createUnsignedInteger(0), createUnsignedInteger(0)}
  ));

  auto airCompileOptions = M.getOrInsertNamedMetadata("air.compile_options");
  airCompileOptions->addOperand(
    MDTuple::get(context, {createString("air.compile.denorms_disable")})
  );
  airCompileOptions->addOperand(MDTuple::get(
    context,
    {opts.enableFastMath ? createString("air.compile.fast_math_enable")
                         : createString("air.compile.fast_math_disable")}
  ));
  airCompileOptions->addOperand(MDTuple::get(
    context, {createString("air.compile.framebuffer_fetch_enable")}
  ));
};

void runOptimizationPasses(llvm::Module &M, llvm::OptimizationLevel opt) {

  // Create the analysis managers.
  // These must be declared in this order so that they are destroyed in the
  // correct order due to inter-analysis-manager references.
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  // Create the new pass manager builder.
  // Take a look at the PassBuilder constructor parameters for more
  // customization, e.g. specifying a TargetMachine or various debugging
  // options.
  PassBuilder PB;

  // Register all the basic analyses with the managers.
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(opt);

  MPM.addPass(VerifierPass());

  // Optimize the IR!
  MPM.run(M, MAM);
}

void Convert(
  const void *dxbc, uint32_t dxbcSize, void **ppAIR, uint32_t *pAIRSize,
  MetalShaderReflection *reflection
) {
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule, {.enableFastMath = true});

  auto ref = dxbc::convertDXBC(dxbc, dxbcSize, context, *pModule);

  runOptimizationPasses(*pModule, OptimizationLevel::O1);

  // pModule->print(outs(), nullptr);

  // Serialize AIR
  SmallVector<char, 0> vec;
  raw_svector_ostream OS(vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  auto ptr = malloc(vec.size());
  memcpy(ptr, vec.data(), vec.size());

  *ppAIR = ptr;
  *pAIRSize = vec.size();

  pModule.reset();

  if (reflection) {
    reflection->has_binding_map = ref.has_binding_map;
  }
}

extern "C" void ConvertDXBC(
  const void *pDXBC, uint32_t DXBCSize, void **ppMetalLib,
  uint32_t *pMetalLibSize, MetalShaderReflection *reflection
) {
  Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize, reflection);
};

} // namespace dxmt