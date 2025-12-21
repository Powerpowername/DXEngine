#include "Utils.h"

static ComPtr<IDxcUtils> g_DxcUtils = nullptr;
static ComPtr<IDxcCompiler3> g_DxcCompiler = nullptr;
static ComPtr<IDxcIncludeHandler> g_DxcIncludeHandler = nullptr;

Shader Utils::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines, const std::wstring &entrypoint, const std::wstring &target)
{

}

ComPtr<ID3D12RootSignature> Utils::CreateRootSignature(ComPtr<ID3D12Device> &device, CD3DX12_ROOT_SIGNATURE_DESC &desc)
{

    ComPtr<ID3D12RootSignature> rootSignature;

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,
                                            serializedRootSig.GetAddressOf(),errorBlob.GetAddressOf());
    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    ThrowIfFailed(hr);

    ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())
    ));
    return rootSignature;
}

ComPtr<ID3D12Resource> Utils::CreateDefaultBuffer(ComPtr<ID3D12Device> &device, ComPtr<ID3D12GraphicsCommandList2> &commandList, const void *initData, UINT64 byteSize, ComPtr<ID3D12Resource> &uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())
    ));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())
    ));

    D3D12_SUBRESOURCE_DATA subReosurceData{};
    subReosurceData.pData = initData;
    subReosurceData.RowPitch = byteSize;
    subReosurceData.SlicePitch = subReosurceData.RowPitch;
    
    commandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(commandList.Get(),defaultBuffer.Get(),uploadBuffer.Get(),0,0,1,&subReosurceData);
    commandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ));
    return defaultBuffer;
}
