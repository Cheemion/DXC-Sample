#include "DX12BaseLine.h"
#include "GlobalDefinition.h"
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void destoryResources()
{
    // wait here for all command completed and destory all resources
    g_ShutDown = true;

    HRESULT hr;
    // wait for all previous submitted commands are done.
    hr = g_pCommandQueue->Signal(g_pFence, ++g_fenceValue);
    hr = g_pFence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
    if (WaitForSingleObject(g_fenceEvent, 2000) == WAIT_FAILED)
    {
        DWORD errorCode = GetLastError();
    }

    g_pDxgiFactory->Release();
}

void initDeviceAndResource()
{
    HRESULT hr;

    // enable debugger layer
    {
        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&g_pDebugerLayer));
        g_pDebugerLayer->EnableDebugLayer();
    }

    hr = CreateDXGIFactory2(g_enableDebuggerLayer ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&g_pDxgiFactory));

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
        hr = g_pDxgiFactory->CreateSwapChainForHwnd(g_pCommandQueue, g_hwnd, &desc, nullptr, nullptr, &temp);
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
    
    // create vertex buffer and upload
    {
        const Vertex vertexData[] = {
            {{0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // top
            {{0.43f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // right
            {{-0.43f,  -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // left
        };

        g_NumVertices = std::size(vertexData);
        
        const CD3DX12_HEAP_PROPERTIES heapPropsDefault { D3D12_HEAP_TYPE_DEFAULT };
        const CD3DX12_HEAP_PROPERTIES heapPropsUpload { D3D12_HEAP_TYPE_UPLOAD };

        const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertexData));
        
        // for GPU read
        hr = g_pDevice->CreateCommittedResource(
            &heapPropsDefault,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&g_pVertexBuffer));
        // for upload vertex buffer
        hr = g_pDevice->CreateCommittedResource(
            &heapPropsUpload,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&g_pVertexUploadBuffer));
        
        // map the upload vertex buffer and upload the vertex data
        Vertex* mappedVertexData = nullptr;
        hr = g_pVertexUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData));
        std::ranges::copy(vertexData, mappedVertexData);
        g_pVertexUploadBuffer->Unmap(0, nullptr);
        
		//reset command allocator and command list
		hr = g_pCommandAllocator->Reset();
		hr = g_pGraphicsCommandList->Reset(g_pCommandAllocator, nullptr);
        g_pGraphicsCommandList->CopyResource(g_pVertexBuffer, g_pVertexUploadBuffer);
        g_pGraphicsCommandList->Close();

        ID3D12CommandList* const commandLists[] = { g_pGraphicsCommandList };
        g_pCommandQueue->ExecuteCommandLists(std::size(commandLists), commandLists);

        //waitting the fence
        hr = g_pCommandQueue->Signal(g_pFence, ++g_fenceValue);
        hr = g_pFence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
        if (WaitForSingleObject(g_fenceEvent, INFINITE) == WAIT_FAILED)
        {
            DWORD errorCode = GetLastError();
        }
    }
    
    // create vertex buffer view
    {
        g_vertexBufferView = {};
        g_vertexBufferView.BufferLocation = g_pVertexBuffer->GetGPUVirtualAddress();
        g_vertexBufferView.SizeInBytes    = g_NumVertices * sizeof(Vertex);
        g_vertexBufferView.StrideInBytes  = sizeof(Vertex);
    }
    
    // init root signature
    { 
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        ID3D10Blob* pSignatureBlob;
        ID3D10Blob* pErrorBlob;

        hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);

        hr = g_pDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_pRootSignature));

        if (pSignatureBlob) pSignatureBlob->Release();
        if (pErrorBlob) pErrorBlob->Release();
    }
    
    // PSO
    {
        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT   InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } pipelineStateStream;

        const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        ID3D10Blob* pVertexShaderBlob;
        hr = D3DReadFileToBlob(L"VertexShader.cso", &pVertexShaderBlob);

        ID3D10Blob* pPixelShaderBlob;
        hr = D3DReadFileToBlob(L"PixelShader.cso", &pPixelShaderBlob);

        pipelineStateStream.RootSignature = g_pRootSignature;
        pipelineStateStream.InputLayout = { inputLayout , std::size(inputLayout) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderBlob);
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderBlob);
        pipelineStateStream.RTVFormats = {
            .RTFormats{ DXGI_FORMAT_R8G8B8A8_UNORM },
            .NumRenderTargets = 1,
        };

        const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
            sizeof(pipelineStateStream), &pipelineStateStream
        };

        hr = g_pDevice->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&g_pPipelineState));
    }
}

void render()
{
    static float t = 0.f;
    constexpr float step = 0.01f;
    t = t + step;

    const CD3DX12_RECT scissorRect{ 0, 0, LONG_MAX, LONG_MAX };

    const CD3DX12_VIEWPORT viewport{ 0.0f, 0.0f, float(Width), float(Height) };

    HRESULT hr;

    UINT curBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    auto& backBuffer = g_backBuffers[curBackBufferIndex];
   
    const auto rtvDescriptorSize = g_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtv{
        g_pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        (INT)curBackBufferIndex,
        rtvDescriptorSize
    };

    //reset command allocator and command list
    hr = g_pCommandAllocator->Reset();
    hr = g_pGraphicsCommandList->Reset(g_pCommandAllocator, nullptr);
    
    // clear rtv
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    
        g_pGraphicsCommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = {
            sin(2.f * t + 1.0f) / 2.f + 0.5f,
            sin(3.f * t + 3.0f) / 2.f + 0.5f,
            sin(5.f * t + 5.0f) / 2.f + 0.5f,
            1.0f};
       

        
        g_pGraphicsCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
    
    // draw
	{
        g_pGraphicsCommandList->SetPipelineState(g_pPipelineState);
        g_pGraphicsCommandList->SetGraphicsRootSignature(g_pRootSignature);
        g_pGraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pGraphicsCommandList->IASetVertexBuffers(0, 1, &g_vertexBufferView);
        g_pGraphicsCommandList->RSSetViewports(1, &viewport);
        g_pGraphicsCommandList->RSSetScissorRects(1, &scissorRect);
        g_pGraphicsCommandList->OMSetRenderTargets(1, &rtv, TRUE, nullptr);

        g_pGraphicsCommandList->DrawInstanced(g_NumVertices, 1, 0, 0);
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

    hr = g_pCommandQueue->Signal(g_pFence, ++g_fenceValue);
    
    // From a driver developer's view, this is in fact, the queue to execute the present, other than the swapchian,
    // Maybe swapChain has to related to a queue during creation.
    // The first paramater in present is VSync,
    // setting it to 1 means using Vsyn
    hr = g_pSwapChain->Present(1, 0);
    
    hr = g_pFence->SetEventOnCompletion(g_fenceValue, g_fenceEvent);
    
    if (WaitForSingleObject(g_fenceEvent, INFINITE) == WAIT_FAILED)
    {
        DWORD errorCode = GetLastError();
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

    initDeviceAndResource();

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if(!g_ShutDown)
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
            destoryResources();
            return 0;
        default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}