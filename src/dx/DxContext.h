#pragma once

#include "dx/dx.h"

#include "CommandQueue.h"
#include "Texture.h"
#include "DescriptorHeap.h"

class DxContext 
{
public:
    DxContext(HWND hwnd,UINT width,UINT height);
    void OnResize(UINT width,UINT height);
    void Present(bool vsync);

    Texture& CurrentBackBuffer(){return m_SwapChainBuffer[m_CurrBackBufferIndex];}


    


private:
    HWND m_hWnd;

    ComPtr<IDXGIFactory4> m_Factory;
    ComPtr<IDXGIAdapter4> m_Adapter;
    ComPtr<ID3D12Device> m_Device;
    std::shared_ptr<CommandQueue> m_CommandQueue = std::make_shared<CommandQueue>();
    
    int m_CurrBackBuffer = 0;
    ComPtr<IDXGISwapChain> m_SwapChain;
    Texture m_SwapChainBuffer[NUM_FRAMES_IN_FLIGHT];
    Texture m_DepthStencilBuffer;
    DescriptorHeap m_RtvHeap;
    DescriptorHeap m_DsvHeap;
    DescriptorHeap m_CbvSrvUavHeap;
    DescriptorHeap m_ImGuiHeap;

	bool m_UseWarp = false;
	bool m_TearingSupported = false;

	UINT m_Width;
	UINT m_Height;

};



