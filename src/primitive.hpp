#pragma once
#include <vector>
#include "primitiveData.hpp"
#include "wrl.h"
#include "d3d11.h"

using namespace Microsoft::WRL;

class Primitive
{
public:
	explicit Primitive(ComPtr<ID3D11Device> device);
	~Primitive() = default;
	Primitive(const Primitive&) = delete;
	Primitive& operator=(const Primitive&) = delete;
	Primitive(Primitive&&) = default;
	Primitive& operator=(Primitive&&) = default;

	void setVertexData(std::vector<InterleavedData>&& vertexData);
	void setIndexData(std::vector<uint32_t>&& indexData);


	struct Triangle
	{
		InterleavedData v0, v1, v2;
	};

	std::vector<Triangle> getTriangles() const;
	const std::vector<InterleavedData>& getVertexData() const { return m_vertexData; }
	const std::vector<uint32_t>& getIndexData() const { return m_indexData; }
	const ComPtr<ID3D11Buffer>& getIndexBuffer() const { return m_indexBuffer; };
	const ComPtr<ID3D11Buffer>& getVertexBuffer() const { return m_vertexBuffer; };

private:
	std::vector<InterleavedData> m_vertexData;
	std::vector<uint32_t> m_indexData;
	ComPtr<ID3D11Device> m_device;

	ComPtr<ID3D11Buffer> m_indexBuffer;
	ComPtr<ID3D11Buffer> m_vertexBuffer;
};