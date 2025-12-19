#include "CommandQueue.h"
#include "directx/d3dx12.h"
CommandQueue::CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
    : m_Device(device), m_CommandListType(type),m_FenceValue(0)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type; 
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//影响的是GPU的调度顺序和时间，对于CPU是间接影响
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;//执行此命令队列的GPU节点


    ThrowIfFailed(m_Device->CreateCommandQueue(&desc,IID_PPV_ARGS(&m_CommandQueue)));
    ThrowIfFailed(m_Device->CreateFence(m_FenceValue,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&m_Fence)));

    m_FenceEvent  = ::CreateEvent(NULL,FALSE,FALSE,NULL);
    assert(m_FenceEvent && "Failed to create fence event.");
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetFreeCommandList()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;//命令分配器分配和管理命令列表的显存空间
    ComPtr<ID3D12GraphicsCommandList2> commandList;
    //存在空闲命令分配器就使用现有的，否则重建新的
    //注意：此处必须将命令分配器和命令列表都重置
    // 命令分配器 Reset：让内存从头开始重用（防止内存持续增长）
    // 命令列表 Reset：让命令列表对象准备好录制新命令
    if(!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().FenceValue))
    {
        commandAllocator = m_CommandAllocatorQueue.front().CommandAllocator;
        m_CommandAllocatorQueue.pop();
        //重置命令列表分配的内存
        ThrowIfFailed(commandAllocator->Reset());
    } 
    else
    {
        commandAllocator = CreateCommandAllocator();
    }

    if(!m_CommandListQueue.empty())
    {
        commandList = m_CommandListQueue.front();
        m_CommandListQueue.pop();
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(),nullptr));
    }
    else
    {
        commandList = CreateCommandList(commandAllocator);
    }
    //此处是给commandList与对应的coammandAllocator做一个映射，这样可以随时知道commandList和那个commandAllocator关联
	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));
    return commandList;
}
//此处我认为可能会出现多个对象使用同一个commandList的情况，因为commandList每次被使用完会被塞回队列，而又没有让外部对象释放所有权
UINT64 CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList2> commandList)
{
    commandList->Close();
    ID3D12CommandAllocator* commandAllocator;
    UINT dataSize = sizeof(commandAllocator);
    ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator),&dataSize,&commandAllocator));

    ID3D12CommandList * const ppCommandLsits[] =
    {
        commandList.Get()
    };

    m_CommandQueue->ExecuteCommandLists(1,ppCommandLsits);
    UINT64 fenceValue = Signal();
    m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, commandAllocator});
    m_CommandListQueue.push(commandList);
	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference
	// in this temporary COM pointer here.
    // 也就是说这里释放的是临时指针，释放这个临时指针减少COM 对象内部的引用计数
    commandAllocator->Release();
    return fenceValue;
}

UINT64 CommandQueue::Signal()
{
    //fenceValue 对应的是执行状态，变化了说名执行状态更新了
    UINT64 fenceValue = ++ m_FenceValue;
    m_CommandQueue->Signal(m_Fence.Get(),fenceValue);
    return fenceValue;
}

bool CommandQueue::IsFenceComplete(UINT64 fenceValue)
{
    return (m_Fence->GetCompletedValue() >= fenceValue);
}
void CommandQueue::WaitForFenceValue(UINT64 fenceValue)
{
    if(!IsFenceComplete(fenceValue))
    {
        m_Fence->SetEventOnCompletion(fenceValue,m_FenceEvent);
        //CPU线程被阻塞住等待GPU执行完毕过后激活事件
        ::WaitForSingleObject(m_FenceEvent,INFINITE);//无线等待
    }
}

void CommandQueue::Flush()
{
    WaitForFenceValue(Signal());
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const
{
    return m_CommandQueue;
}
//创建命令分配器，如果没有接收对象会自动析构掉自身
ComPtr<ID3D12CommandAllocator>  CommandQueue::CreateCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator>  commandAllocator;
	ThrowIfFailed(m_Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(CommandAllocator allocator)
{
    ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(m_Device->CreateCommandList(0, m_CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    return commandList;
}