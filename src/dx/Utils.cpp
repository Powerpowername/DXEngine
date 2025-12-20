#include "Utils.h"

static ComPtr<IDxcUtils> g_DxcUtils = nullptr;
static ComPtr<IDxcCompiler3> g_DxcCompiler = nullptr;
static ComPtr<IDxcIncludeHandler> g_DxcIncludeHandler = nullptr;

Shader Utils::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines, const std::wstring &entrypoint, const std::wstring &target)
{

}

ComPtr<ID3D12RootSignature> Utils::CreateRootSignature(ComPtr<ID3D12Device> &device, CD3DX12_ROOT_SIGNATURE_DESC &desc)
{




}

ComPtr<ID3D12Resource> Utils::CreateDefaultBuffer(ComPtr<ID3D12Device> &device, ComPtr<ID3D12GraphicsCommandList2> &commandList, const void *initData, UINT64 byteSize, ComPtr<ID3D12Resource> &uploadBuffer)
{

}
