#pragma once
#include <vector>
#include "primitiveData.hpp"
#include "wrl.h"
#include "d3d11.h"
#include <memory_resource>

using namespace Microsoft::WRL;

class Primitive
{
public:
	Primitive(ComPtr<ID3D11Device>& device);
	~Primitive() = default;

	void setVertexData(const std::vector<InterleavedData>& posData);
	void* operator new(size_t size);
	void operator delete(void* ptr) noexcept;

private:
	inline static std::pmr::unsynchronized_pool_resource m_primitiveAllocator{};
	std::vector<InterleavedData> m_vertexData;
	ComPtr<ID3D11Device>& m_device;

	ComPtr<ID3D11Buffer> m_indexBuffer;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
};