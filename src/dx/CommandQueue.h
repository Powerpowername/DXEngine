#pragma once

#include "dx.h"


class CommandQueue
{
public:
    CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);

    ComPtr<ID3D12GraphicsCommandList2> GetFreeCommandList();

    UINT64 ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList);

    UINT64 Signal();


    bool IsFenceComplete(UINT64 fenceValue);
    void WaitForFenceValue(UINT64 fenceValue);
    void Flush();

    ComPtr<ID3D12CommandQueue> GetCommandQueue() const;


private:
    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(CommandAllocator commandAllocator);


private:
    struct CommandAllocatorEntry
    {
        UINT64 FenceValue;
        CommandAllocator CommandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<GraphicsCommandList>;
	D3D12_COMMAND_LIST_TYPE     m_CommandListType;
	ComPtr<ID3D12Device> 		m_Device;
	ComPtr<ID3D12CommandQueue>  m_CommandQueue;
	ComPtr<ID3D12Fence>		    m_Fence;
	HANDLE                      m_FenceEvent;
	UINT64                      m_FenceValue;

	CommandAllocatorQueue       m_CommandAllocatorQueue;
	CommandListQueue            m_CommandListQueue;
};

