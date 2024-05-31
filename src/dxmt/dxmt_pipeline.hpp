
#pragma once
#include "objc_pointer.hpp"
#include "rc/util_rc.hpp"
#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
namespace dxmt {

struct RenderPipelineCacheEntry {
  void *VertexShaderRef;
  void *PixelShaderRef;

  bool operator==(const RenderPipelineCacheEntry &other) const {
    return VertexShaderRef == other.VertexShaderRef &&
           PixelShaderRef == other.PixelShaderRef;
  }
};

class RenderPipelineCache : public RcObject {
public:
  Obj<MTL::RenderPipelineState> PSO;

  RenderPipelineCache(MTL::RenderPipelineState *pso)
      : PSO(pso) {}
};

RenderPipelineCache *findCache(RenderPipelineCacheEntry &key);
void setCache(RenderPipelineCacheEntry &key, RenderPipelineCache *value);

} // namespace dxmt

template <> struct std::hash<dxmt::RenderPipelineCacheEntry> {
  std::size_t operator()(const dxmt::RenderPipelineCacheEntry &p) const {
    return std::hash<void *>()(p.VertexShaderRef) ^
           std::hash<void *>()(p.PixelShaderRef);
  }
};