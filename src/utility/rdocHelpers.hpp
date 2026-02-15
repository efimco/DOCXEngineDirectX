#pragma once

#ifndef ENABLE_RENDERDOC_CAPTURE
#define ENABLE_RENDERDOC_CAPTURE 1
#endif

#if defined(_DEBUG) && ENABLE_RENDERDOC_CAPTURE

#include <wrl.h>
#include "dxDevice.hpp"

class RenderDocScope
{
public:
	RenderDocScope(ComPtr<ID3D11Device> device);
	~RenderDocScope();

private:
	RenderDocScope(RenderDocScope&&) = delete;
	RenderDocScope(const RenderDocScope&) = delete;
	RenderDocScope& operator =(RenderDocScope&&) = delete;
	RenderDocScope& operator =(const RenderDocScope&) = delete;

	ComPtr<ID3D11Device> m_device;
};

#endif