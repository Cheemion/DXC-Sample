#include <windows.h>
#include "DX12BaseLine.h"
#include "GlobalDefinition.h"
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void initDevice()
{
    HRESULT hr;

    IDXGIFactory4* pDxgiFactory;

    hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&pDxgiFactory));

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&g_pDevice));

    // command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
        
        hr = g_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pCommandQueue));
    }

    // swap chain
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = Width;
        desc.Height = Height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = bufferCount;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags = 0;
        // Need a CommandQueue for SwapChain
        IDXGISwapChain1* temp;
        hr = pDxgiFactory->CreateSwapChainForHwnd(g_pCommandQueue, g_hwnd, &desc, nullptr, nullptr, &temp);
        g_pSwapChain = static_cast<IDXGISwapChain4*>(temp);
    }
    
    // create rtv descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = bufferCount;

        hr = g_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pRtvDescriptorHeap));
    }
    
	const auto rtvDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // create rtv descriptor and backbuffer resource
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        for (int i = 0; i < bufferCount; i++)
        {
            hr = g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_backBuffers[i]));
            g_pDevice->CreateRenderTargetView(g_backBuffers[i], nullptr, rtvHandle);
            rtvHandle.Offset(rtvDescriptorSize);
        }
    }

    //create command allocator and command list
    {
        hr = g_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocator));
        hr = g_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_pCommandAllocator, nullptr, IID_PPV_ARGS(&g_pGraphicsCommandList));
        
        // close the command list at the beginning, in order to be better handle in our code.
        hr = g_pGraphicsCommandList->Close();
    }
    
    // create a fence and fenceEvent
    {
        hr = g_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pFence));
        
        g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }

    pDxgiFactory->Release();
}

void render()
{
    HRESULT hr;

    UINT curBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    auto& backBuffer = g_backBuffers[curBackBufferIndex];
    
    //reset command allocator and command list
    hr = g_pCommandAllocator->Reset();
    hr = g_pGraphicsCommandList->Reset(g_pCommandAllocator, nullptr);
    
    // clear rtv
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    
        g_pGraphicsCommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
       
        const auto rtvDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv{
            g_pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            (INT)curBackBufferIndex,
            rtvDescriptorSize
        };
        
        g_pGraphicsCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    // prepare buffer for presentation
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, 
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    
        g_pGraphicsCommandList->ResourceBarrier(1, &barrier);
    }
    
    // submit
    {
        hr = g_pGraphicsCommandList->Close();
        ID3D12CommandList* const commandlists[] = { g_pGraphicsCommandList };
        g_pCommandQueue->ExecuteCommandLists(std::size(commandlists), commandlists);
    }

    hr = g_pCommandQueue->Signal(g_pFence, g_fenceValue++);
    
    // From a driver developer's view, this is in fact, the queue to execute the present, other than the swapchian,
    // Maybe swapChain has to related to a queue during creation.
    hr = g_pSwapChain->Present(0, 0);
    
    hr = g_pFence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
    
    if (WaitForSingleObject(g_fenceEvent, INFINITE) == WAIT_FAILED)
    {
        GetLastError();
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"DXC Sample";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Create the window.
    g_hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"DXC Sample Box",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, Width, Height,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    ShowWindow(g_hwnd, nCmdShow);

    initDevice();

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        render();
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // All painting occurs here, between BeginPaint and EndPaint.

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        EndPaint(hwnd, &ps);
    }
    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}