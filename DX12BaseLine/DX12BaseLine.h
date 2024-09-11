#pragma once
#include <Windows.h>
#include <d3d12.h>
#include "d3dx12.h"

// create swap chain
#include <dxgi1_6.h>

#include <DirectXMath.h>

#include <cmath>
#include <numbers>
#include <ranges>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
};


