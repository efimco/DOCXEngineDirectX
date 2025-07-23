#include "primitive.hpp"
#include "assert.h"

Primitive::Primitive(ComPtr<ID3D11Device> device) : m_device(device) {}

void Primitive::setVertexData(std::vector<InterleavedData>&& vertexData)
{
	m_vertexData = std::move(vertexData);
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

void Primitive::setIndexData(std::vector<uint32_t>&& indexData)
{
	m_indexData = std::move(indexData);
	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = static_cast<UINT>(m_indexData.size() * sizeof(uint32_t));
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexInitData = {};
	indexInitData.pSysMem = m_indexData.data();
	HRESULT hr = m_device->CreateBuffer(&indexBufferDesc, &indexInitData, &m_indexBuffer);
	assert(SUCCEEDED(hr));
}

std::vector<Primitive::Triangle> Primitive::getTriangles() const
{
	std::vector<Primitive::Triangle> triangles;
	triangles.reserve(m_indexData.size() / 3);

	for (size_t i = 0; i < m_indexData.size(); i += 3)
	{
		if (i + 2 < m_indexData.size())
		{
			Primitive::Triangle tri;
			tri.v0 = m_vertexData[m_indexData[i]];
			tri.v1 = m_vertexData[m_indexData[i + 1]];
			tri.v2 = m_vertexData[m_indexData[i + 2]];
			triangles.push_back(tri);
		}
	}

	return triangles;
}


