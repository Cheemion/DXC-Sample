#pragma once
constexpr UINT Width = 1280;
constexpr UINT Height = 720;
constexpr UINT bufferCount = 2;
bool g_enableDebuggerLayer = true;
bool g_ShutDown = false;
HWND g_hwnd;
ID3D12Debug* g_pDebugerLayer;
ID3D12Device2* g_pDevice;
ID3D12CommandQueue* g_pCommandQueue;
IDXGISwapChain4* g_pSwapChain;
ID3D12DescriptorHeap* g_pRtvDescriptorHeap;
ID3D12Resource* g_backBuffers[bufferCount];
ID3D12CommandAllocator* g_pCommandAllocator;
ID3D12GraphicsCommandList* g_pGraphicsCommandList;
ID3D12Fence* g_pFence;
IDXGIFactory4* g_pDxgiFactory;
uint64_t g_fenceValue = 0;
HANDLE g_fenceEvent;
UINT g_NumVertices;

ID3D12Resource* g_pVertexBuffer;
ID3D12Resource* g_pVertexUploadBuffer;
D3D12_VERTEX_BUFFER_VIEW g_vertexBufferView;
ID3D12RootSignature* g_pRootSignature;
ID3D12PipelineState* g_pPipelineState;
