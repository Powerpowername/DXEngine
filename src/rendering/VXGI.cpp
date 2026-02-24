#include "VXGI.h"
#include "PipelineStates.h"

VXGI::VXGI(shared_ptr<DxContext> dxContext, UINT size)
{
    m_Device = dxContext->GetDevice();
    m_ViewPort = {0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f};
    m_ScissorRect = {0, 0, (int)size, (int)size};
    
    m_MipLevels = 1;
    //计算mip levels 层级
    while (size >> m_MipLevels)
        m_MipLevels++;
    m_TextureUav.resize(m_MipLevels * 2);

    // allocate descriptors
    // 此处的堆内存已近在DxContext中创建好了，所以可以直接分配，但是存在数量限的
    m_TextureSrv[0] = dxContext->GetCbvSrvUavHeap().Alloc();
    m_TextureSrv[1] = dxContext->GetCbvSrvUavHeap().Alloc();
    for (int i = 0; i < m_MipLevels * 2; i++)
        m_TextureUav[i] = dxContext->GetCbvSrvUavHeap().Alloc();

    m_VoxelBufferUav = dxContext->GetCbvSrvUavHeap().Alloc();

    // build resources and descriptors
    this->OnResize(size, size, size);
    // initialize the voxel buffer to zero
    auto commandList = dxContext->GetCommandList();

    ID3D12DescriptorHeap *descriptorHeaps[] = {dxContext->GetCbvSrvUavHeap().Get()};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetPipelineState(PipelineStates::GetPSO("clearVoxel"));
    //
    commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());
    commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 1, &m_VoxelBufferUav.Index, 0);

    UINT groupSize = size / 8;
    commandList->Dispatch(groupSize, groupSize, groupSize);

    dxContext->ExecuteCommandList();
    dxContext->Flush();
}

void VXGI::OnResize(UINT x,UINT y,UINT z)
{
    if(m_X == x && m_Y == y && m_Z == z)
        return;
    m_X = x;
    m_Y = y;
    m_Z = z;
    BuildResources();
    BuildDescriptors();
}

void VXGI::BufferToTexture3D(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    UINT resources[2] = { m_VoxelBufferUav.Index, m_TextureSrv[0].Index };
    commandList->SetPipelineState(PipelineStates::GetPSO("voxelBuffer2Tex"));
    //通过CD3DX12_ROOT_PARAMETER::InitAsConstantBufferView将数据与hlsl中的寄存器绑定，
    //IID_ID3D12GraphicsCommandList::SetComputeRoot32BitConstants可通过设置根参数间接设置寄存器数据
    commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 2, resources, 0);
    UINT groupSize = std::max(VOXEL_DIMENSION / 8, 1);
    commandList->Dispatch(groupSize, groupSize, groupSize);

    // generate mipmap for the first texture
    GenVoxelMipmap(commandList, 0);

    // write the second bounce color to the second texture
    ComputeSecondBound(commandList);

    // generate mipmap for the second texture
    GenVoxelMipmap(commandList, 1);


}

void VXGI::GenVoxelMipmap(ComPtr<ID3D12GraphicsCommandList2> commandList, int index)
{
    commandList->SetPipelineState(PipelineStates::GetPSO("voxelMipmap"));

    for (int i = 1, levelWidth = VOXEL_DIMENSION / 2; i < m_MipLevels; i++, levelWidth /= 2)
    {
        //第一个纹理的mip level 0 到 level m_MipLevels-1 作为输入
        //第二个纹理的mip level m_MipLevels 到 level 2*m_MipLevels-1 作为输入
        //两个纹理的mipmap层级是一样多的0，m_MipLevels-1
        UINT resources[] = {m_TextureUav[index * m_MipLevels + i - 1].Index,
                            m_TextureUav[index * m_MipLevels + i].Index};
        UINT count = std::max(levelWidth / 8, 1);
        commandList->Dispatch(count, count, count);
    }
}

void VXGI::ComputeSecondBound(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    UINT resources[] = {m_VoxelBufferUav.Index,
                        m_TextureSrv[0].Index,
                        m_TextureUav[m_MipLevels].Index}; // second 3d texture uav (first mip level)

    


}
