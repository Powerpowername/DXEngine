#pragma once

#include "dx/dx.h"
#include "pch.h"

enum struct RootParam : UINT
{
    ObjectCB = 0,
    PassCB,
    MatCB,
    LightCB,
    ShadowCB,
    SSAOCB,
    RenderResources,
    Count
};

class PipelineStates
{
public:
    static void Init(ComPtr<ID3D12Device> device);
    static void Cleanup();
    static ID3D12RootSignature *GetRootSignature() { return m_RootSignature.Get(); }
    static ID3D12PipelineState *GetPSO(const std::string &name) { return m_PSOs[name].Get(); }


private:
    static void BuildRootSignature(ComPtr<ID3D12Device> device);
    static void BuildPSOs(ComPtr<ID3D12Device> device);
    static std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSamplers();


private:
    static ComPtr<ID3D12RootSignature> m_RootSignature;
    static std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

};
