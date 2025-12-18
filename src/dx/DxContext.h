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

    Texture& CurrentBackBuffer(){return m_SwapChainBuffer[m_CurrBackBuffer];}

    Texture& DepthStencilBuffer(){return m_DepthStencilBuffer;}

    ComPtr<ID3D12Device> GetDevice(){return m_Device;}

    DescriptorHeap& GetRtvHeap(){return m_RtvHeap;}

    DescriptorHeap& GetDsvHeap(){return m_DsvHeap;}

    DescriptorHeap& GetCbvSrvUavHeap(){return m_CbvSrvUavHeap;}

    DescriptorHeap& GetImGuiHeap() { return m_ImGuiHeap; }

    ComPtr<ID3D12GraphicsCommandList2>& GetCommandList();

    UINT64 ExecuteCommandList();

    UINT64 Signal(){return m_CommandQueue->Signal();}

    void WaitForFenceValue(UINT64 fenceValue){m_CommandQueue->WaitForFenceValue(fenceValue);}

    void Flush() {m_CommandQueue->Flush();}

private:
	void EnableDebugLayer();
	void CreateDXGIFactory();
	void GetAdapter();
	void CreateDevice();
	void CheckTearingSupport();
	void CreateSwapChain();

	void CreateDescriptorHeaps();
	void AllocateDescriptors();

	void ResizeSwapChain();
	void ResizeDepthStencilBuffer(GraphicsCommandList commandList);


private:
    HWND m_hWnd;

    ComPtr<IDXGIFactory4> m_Factory;
    ComPtr<IDXGIAdapter4> m_Adapter;
    ComPtr<ID3D12Device> m_Device;
    std::shared_ptr<CommandQueue> m_CommandQueue = std::make_shared<CommandQueue>();
    ComPtr<ID3D12GraphicsCommandList2> m_ActiveCommandList = nullptr;

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



