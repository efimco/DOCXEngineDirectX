#include "primitive.hpp"
#include "assert.h"

Primitive::Primitive(ComPtr<ID3D11Device>& device) : m_device(device) {}

void Primitive::setVertexData(const std::vector<InterleavedData>& vertexData)
{
	m_vertexData = vertexData;
	auto numVerts = m_vertexData.size();
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(numVerts * sizeof(InterleavedData));
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vertexSubresourceData;
	vertexSubresourceData.pSysMem = m_vertexData.data();
	vertexSubresourceData.SysMemPitch = 0;
	vertexSubresourceData.SysMemSlicePitch = 0;
	HRESULT hr = m_device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &m_vertexBuffer);
	assert(SUCCEEDED(hr));
}

void* Primitive::operator new(size_t size)
{
	return m_primitiveAllocator.allocate(size, alignof(Primitive));
}

void Primitive::operator delete(void* ptr) noexcept
{
	m_primitiveAllocator.deallocate(ptr, sizeof(Primitive), alignof(Primitive));
}
