#include "pch.h"
#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Descriptor.h"

#define VOXEL_DIMENSION 256 
using std::shared_ptr;
struct Voxel
{
    UINT64 Radiance;
    UINT64 Normal;
};

class VXGI
{
public:
    VXGI(shared_ptr<DxContext> dxContext,UINT size);
    void OnResize(UINT x,UINT y,UINT z);

    D3D12_VIEWPORT &GetViewPort() { return m_ViewPort; }
    D3D12_RECT &GetScissorRect() { return m_ScissorRect; }

    void BufferToTexture3D(ComPtr<ID3D12GraphicsCommandList2> commandList);


private:
    /// @brief 
    /// @param commandList 
    /// @param index index = 0：处理第一个3D纹理（用于直接光照）index = 1：处理第二个3D纹理（用于第二次反弹光照）
    void GenVoxelMipmap(ComPtr<ID3D12GraphicsCommandList2> commandList, int index);
    void ComputeSecondBound(ComPtr<ID3D12GraphicsCommandList2> commandList);

private:
    ComPtr<ID3D12Device> m_Device = nullptr;
    ComPtr<ID3D12Resource> m_VoxelBuffer = nullptr;
    Descriptor m_VoxelBufferUav;

    ComPtr<ID3D12Resource> m_VolumeTexture[2];

    DXGI_FORMAT m_TextureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    UINT m_MipLevels = 0;

    Descriptor m_TextureSrv[2];
    std::vector<Descriptor> m_TextureUav;

    UINT m_X = -1, m_Y = -1, m_Z = -1;
    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;
};

