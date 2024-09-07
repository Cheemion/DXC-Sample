#pragma once
constexpr UINT Width = 1280;
constexpr UINT Height = 720;
constexpr UINT bufferCount = 2;
HWND g_hwnd;
ID3D12Device2* g_pDevice;
ID3D12CommandQueue* g_pCommandQueue;
IDXGISwapChain4* g_pSwapChain;
ID3D12DescriptorHeap* g_pRtvDescriptorHeap;
ID3D12Resource* g_backBuffers[bufferCount];
ID3D12CommandAllocator* g_pCommandAllocator;
ID3D12GraphicsCommandList* g_pGraphicsCommandList;
ID3D12Fence* g_pFence;
uint64_t g_fenceValue = 0;
HANDLE g_fenceEvent;
