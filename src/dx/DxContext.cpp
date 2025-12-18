#include "pch.h"
#include "DxContext.h"

DxContext::DxContext(HWND hWnd, UINT width, UINT height)
	: m_hWnd(hWnd), m_Width(width), m_Height(height)
{


}

void DxContext::Present(bool vsync)
{
    int sync = vsync ? 1 : 0;
    int flag = vsync ? 0 : (m_TearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);

    ThrowIfFailed(m_SwapChain->Present(sync, flag));

    m_CurrBackBuffer = (m_CurrBackBuffer + 1) % NUM_FRAMES_IN_FLIGHT;

}


ComPtr<ID3D12GraphicsCommandList2>&  DxContext::GetCommandList()
{
    if(!m_ActiveCommandList)
        m_ActiveCommandList = m_CommandQueue->GetFreeCommandList();

    return m_ActiveCommandList;
}

UINT64 DxContext::ExecuteCommandList()
{
	ASSERT(m_ActiveCommandList, "No Active Command List.");

    UINT64 newFenceValue = m_CommandQueue->ExecuteCommandList(m_ActiveCommandList);

	m_ActiveCommandList = nullptr;
	return newFenceValue;
}

void DxContext::EnableDebugLayer()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void DxContext::CreateDXGIFactory()
{
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags,IID_PPV_ARGS(&m_Factory)));
}


void DxContext::CreateDevice()
{
    ThrowIfFailed(D3D12CreateDevice(
        m_Adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_Device)
    ));
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(m_Device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		// D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,						  // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,					  // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		// NewFilter.DenyList.NumCategories = _countof(Categories);
		// NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(infoQueue->PushStorageFilter(&NewFilter));
	}
#endif
}

void DxContext::CheckTearingSupport()
{
    BOOL allowTearing = FALSE;
    ComPtr<IDXGIFactory5> factory5;
    if(SUCCEEDED(m_Factory.As(&factory5)))
    {
        if(FAILED(factory5->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allowTearing,
            sizeof(allowTearing)
        )))
        {
            allowTearing = FALSE;
        }
    }
    m_TearingSupported = allowTearing == TRUE;

    LOG_INFO("Tearing Supported: {0}", m_TearingSupported);
}

void DxContext::CreateSwapChain()
{
    m_SwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC1 desc{};

    desc.Width = m_Width;
    desc.Height = m_Height;
    desc.Format = BACK_BUFFER_FORMAT;
    desc.Stereo = FALSE;
    desc.SampleDesc = {1, 0};
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = NUM_FRAMES_IN_FLIGHT;
    desc.Scaling = DXGI_SCALING_STRETCH;//伸缩策略
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//翻转丢弃，更高效
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 
}