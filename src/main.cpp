#ifndef UNICODE
#define UNICODE
#endif 

#include "window.hpp"
#include <wrl.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <assert.h>
#include <DirectXMath.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>

using namespace Microsoft::WRL;
using namespace DirectX;
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	Window window(hInstance);

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_1 };
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);


	ComPtr<ID3D11Device> d3dDevice;
	ComPtr<ID3D11DeviceContext> d3dDeviceContext;
	UINT deviceCreationFlags = 0;
#if defined(_DEBUG)
	// If the project is in a debug build, enable the debug layer.
	deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,					// IDXGIAdapter* pAdapter
			D3D_DRIVER_TYPE_HARDWARE,	//D3D_DRIVER_TYPE DriverType 
			nullptr,					//HMODULE Software
			deviceCreationFlags,		//UINT flags
			featureLevels,				//CONST D3D_FEATURE_LEVEL* pFeatureLevels
			numFeatureLevels,			//UINT FeatureLevels
			D3D11_SDK_VERSION,			//UINT SDKVersion,
			&d3dDevice,					//_COM_Outptr_opt_ ID3D11Device** ppDevice
			nullptr,					//D3D_FEATURE_LEVEL* pFeatureLevel
			&d3dDeviceContext);			//ID3D11DeviceContext** ppImmediateContext
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	{
		HRESULT hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice);
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIAdapter> dxgiAdapter;
	{
		HRESULT hr = dxgiDevice->GetAdapter(&dxgiAdapter);
		assert(SUCCEEDED(hr));
	}

	ComPtr<IDXGIFactory> dxgiFactory;
	{
		HRESULT hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11RasterizerState> rasterizerState;
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_BACK;
		rasterizerDesc.FrontCounterClockwise = false;

		d3dDevice->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
	}

	ComPtr<ID3D11DepthStencilState> depthStencilState;
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		d3dDevice->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
	}


	ComPtr<IDXGISwapChain> dxgiSwapChain;
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

		swapChainDesc.BufferDesc.Width = 0;
		swapChainDesc.BufferDesc.Height = 0;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;
		swapChainDesc.OutputWindow = window.getHandle();
		swapChainDesc.Windowed = true;
		HRESULT hr = dxgiFactory->CreateSwapChain(d3dDevice.Get(), &swapChainDesc, &dxgiSwapChain);
		assert(SUCCEEDED(hr));
	}

	ComPtr<ID3D11Texture2D> backBuffer;
	{
		HRESULT hr = dxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
		assert(SUCCEEDED(hr));

	}

	ComPtr<ID3D11RenderTargetView> renderTargetView;
	{
		HRESULT hr = d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
		assert(SUCCEEDED(hr));
	}


	struct Vertex
	{
		float x, y, z, u, v;
		Vertex(float _x, float _y, float _z, float _u, float _v) :x(_x), y(_y), z(_z), u(_u), v(_v) {};
	};

	std::vector<Vertex> vBuf =
	{
		// Front face
		{-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, // 0
		{ 0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, // 1
		{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, // 2
		{-0.5f,  0.5f,  0.5f, 0.0f, 0.0f}, // 3

		// Back face
		{ 0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, // 4
		{-0.5f, -0.5f, -0.5f, 1.0f, 1.0f}, // 5
		{-0.5f,  0.5f, -0.5f, 1.0f, 0.0f}, // 6
		{ 0.5f,  0.5f, -0.5f, 0.0f, 0.0f}  // 7
	};

	std::vector<unsigned int> indices =
	{
		// Front face
		0, 1, 2, 0, 2, 3,
		// Back face
		4, 5, 6, 4, 6, 7,
		// Left face
		5, 0, 3, 5, 3, 6,
		// Right face
		1, 4, 7, 1, 7, 2,
		// Top face
		3, 2, 7, 3, 7, 6,
		// Bottom face
		5, 4, 1, 5, 1, 0
	};

	ComPtr<ID3D11Buffer> indexBuffer;
	{
		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexInitData = {};
		indexInitData.pSysMem = indices.data();
		HRESULT hr = d3dDevice->CreateBuffer(&indexBufferDesc, &indexInitData, &indexBuffer);
		assert(SUCCEEDED(hr));
	}

	size_t numIndices = indices.size();
	ComPtr<ID3D11Buffer> vertexBuffer;
	size_t numVert = vBuf.size();

	{
		D3D11_BUFFER_DESC vertexBufferDesc = {};
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = static_cast<UINT>(vBuf.size() * sizeof(Vertex));
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;


		D3D11_SUBRESOURCE_DATA vertexInitData;
		vertexInitData.pSysMem = &vBuf[0];
		vertexInitData.SysMemPitch = 0;
		vertexInitData.SysMemSlicePitch = 0;
		HRESULT hr = d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexInitData, &vertexBuffer);
		assert(SUCCEEDED(hr));
	}


	D3D11_INPUT_ELEMENT_DESC inputDesc[] =
	{
		{"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	// struct D3D11_INPUT_ELEMENT_DESC
	// {
	// LPCSTR SemanticName;
	// UINT SemanticIndex;
	// DXGI_FORMAT Format;
	// UINT InputSlot;
	// UINT AlignedByteOffset;
	// D3D11_INPUT_CLASSIFICATION InputSlotClass;
	// UINT InstanceDataStepRate;
	// } 


	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11InputLayout> inputLayout;
	{ // shader Creation


		ComPtr<ID3DBlob> vsBlob;

		{
			ComPtr<ID3DBlob> shaderCompileErrorsBlob;
			HRESULT hr = D3DCompileFromFile(L"shaders/main.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &shaderCompileErrorsBlob);
			if (FAILED(hr))
			{
				const char* errorString = NULL;
				if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
				{
					errorString = "Could not compile shader; file not found";
				}
				else if (shaderCompileErrorsBlob)
				{
					errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
				}
				std::cout << errorString << std::endl;
			}
			assert(SUCCEEDED(hr));
			hr = d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
			assert(SUCCEEDED(hr));
		}

		{
			HRESULT hr = d3dDevice->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
			assert(SUCCEEDED(hr));
		}


		ComPtr<ID3DBlob> psBlob;

		{
			ComPtr<ID3DBlob> shaderCompileErrorsBlob;
			HRESULT hr = D3DCompileFromFile(L"shaders/main.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &shaderCompileErrorsBlob);
			if (FAILED(hr))
			{
				const char* errorString;
				if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
				{
					errorString = "Could not compile shader; file not found";
				}
				else if (shaderCompileErrorsBlob)
				{
					errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
				}
			}
			assert(SUCCEEDED(hr));
			hr = d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
			assert(SUCCEEDED(hr));
		}
	}


	ComPtr<ID3D11Buffer> constantbuffer;

	struct ConstantBufferData
	{
		XMMATRIX modelViewProjection;
	};

	// Initialize camera matrices
	XMVECTOR cameraPosition = XMVectorSet(0.0f, 0.0f, -2.0f, 1.0f);
	XMVECTOR cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX cameraView = XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	XMMATRIX cameraProjection = XMMatrixPerspectiveFovLH(XM_PIDIV2, (float)window.width / (float)window.height, 0.1f, 100.0f);
	XMMATRIX model = XMMatrixIdentity();
	model = XMMatrixTranslation(0.0f, 0.0f, 10.0f);

	XMMATRIX modelViewProjection = XMMatrixMultiply(XMMatrixMultiply(model, cameraView), cameraProjection);

	ConstantBufferData cbdata;
	cbdata.modelViewProjection = XMMatrixTranspose(modelViewProjection);

	{
		D3D11_BUFFER_DESC constantBufferDesc;
		constantBufferDesc.ByteWidth = sizeof(ConstantBufferData);
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.StructureByteStride = 0;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA constantBufferResourceData;
		constantBufferResourceData.pSysMem = &cbdata;
		constantBufferResourceData.SysMemSlicePitch = 0;
		constantBufferResourceData.SysMemPitch = 0;
		HRESULT hr = d3dDevice->CreateBuffer(&constantBufferDesc, &constantBufferResourceData, &constantbuffer);
		assert(SUCCEEDED(hr));
	}



	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(window.width);
	viewport.Height = static_cast<float>(window.height);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	d3dDeviceContext->RSSetViewports(1, &viewport);


	std::chrono::system_clock::time_point prevTime = std::chrono::system_clock::now();
	std::chrono::duration<double> deltaTime;
	MSG message = {};
	while (message.message != WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_SIZE)
			{
				// Handle window resize
				if (dxgiSwapChain)
				{
					renderTargetView.Reset();
					backBuffer.Reset();

					HRESULT hr = dxgiSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
					assert(SUCCEEDED(hr));

					hr = dxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
					assert(SUCCEEDED(hr));

					hr = d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
					assert(SUCCEEDED(hr));

					viewport.Width = static_cast<float>(window.width);
					viewport.Height = static_cast<float>(window.height);
					d3dDeviceContext->RSSetViewports(1, &viewport);

					cameraProjection = XMMatrixPerspectiveFovLH(XM_PIDIV2, (float)window.width / (float)window.height, 0.1f, 100.0f);
					modelViewProjection = XMMatrixMultiply(XMMatrixMultiply(model, cameraView), cameraProjection);

					D3D11_MAPPED_SUBRESOURCE mappedResource;
					hr = d3dDeviceContext->Map(constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
					if (SUCCEEDED(hr))
					{
						ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
						cbData->modelViewProjection = XMMatrixTranspose(modelViewProjection);
						d3dDeviceContext->Unmap(constantbuffer.Get(), 0);

					}
				}
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
		deltaTime = currentTime - prevTime;
		// std::cout << deltaTime.count() << std::endl;
		prevTime = currentTime;

		static float rotationY = 0.0f;
		rotationY += static_cast<float>(deltaTime.count());
		model = XMMatrixRotationRollPitchYaw(0, rotationY, 0);
		modelViewProjection = XMMatrixMultiply(XMMatrixMultiply(model, cameraView), cameraProjection);
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = d3dDeviceContext->Map(constantbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr))
		{
			ConstantBufferData* cbData = static_cast<ConstantBufferData*>(mappedResource.pData);
			cbData->modelViewProjection = XMMatrixTranspose(modelViewProjection);
			d3dDeviceContext->Unmap(constantbuffer.Get(), 0);
		}

		d3dDeviceContext->RSSetState(rasterizerState.Get());
		d3dDeviceContext->OMSetDepthStencilState(depthStencilState.Get(), 0);
		d3dDeviceContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
		float clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f }; //blue
		d3dDeviceContext->ClearRenderTargetView(renderTargetView.Get(), clearColor);

		d3dDeviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
		d3dDeviceContext->VSSetConstantBuffers(0, 1, constantbuffer.GetAddressOf());

		d3dDeviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);

		d3dDeviceContext->IASetInputLayout(inputLayout.Get());
		d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		d3dDeviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
		d3dDeviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		d3dDeviceContext->DrawIndexed(static_cast<UINT>(numIndices), 0, 0);

		dxgiSwapChain->Present(1, 0);



	}
	return 0;
}


