#include "CommandQueue.h"
#include "directx/d3dx12.h"
CommandQueue::CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
    : m_Device(device), m_CommandListType(type),m_FenceValue(0)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    CD3DX12_COMMAND_QUEUE_DESC 

}

