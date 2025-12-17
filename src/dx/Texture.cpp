#include "Texture.h"

Texture Texture::Create(ComPtr<ID3D12Device> device,UINT width ,UINT height,UINT depth,DXGI_FORMAT format,UINT levels)
{
    ASSERT(depth == 1 || depth == 6, "Texture depth not equal to 1 or 6: depth = {}", depth);

    Texture texture;
    texture.Width = width;
    texture.Height = height;
    texture.Levels = (levels > 0) ? levels : NumMipmapLevels(width, height);

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = depth;
    desc.MipLevels = levels;
    desc.Format = format;
    desc.SampleDesc = {1, 0};
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&texture.Resource)
    ));

    return texture;
}

Texture Texture::Create(ComPtr<ID3D12Device> device, GraphicsCommandList commandList, shared_ptr<Image> &image, DXGI_FORMAT format, UINT levels)
{
    //创建空白资源
    Texture texture = Create(device, image->Width(), image->Height(), 1, format, levels);

    UINT64 textureUploadBufferSize = 0;

	device->GetCopyableFootprints(&texture.Resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(texture.UploadHeap.GetAddressOf())
    ));
    
    D3D12_SUBRESOURCE_DATA sub = {};
	sub.pData = image->Pixels<void>();
	sub.RowPitch = image->Pitch();
	sub.SlicePitch = image->Pitch() * image->Height();
	UpdateSubresources(commandList.Get(), texture.Resource.Get(), texture.UploadHeap.Get(), 0, 0, 1, &sub);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

	return texture;
}
void Texture::Resize(Device device, UINT width, UINT height)
{
    
}
UINT Texture::NumMipmapLevels(UINT width, UINT height)
{
	UINT levels = 1;
	while ((width | height) >> levels)
		levels++;
	return levels;
}