#pragma once
#include <d3d11.h>
inline static const D3D11_INPUT_ELEMENT_DESC genericInputLayoutDesc[] =
{
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

struct Vertex
{
	float x, y, z;
	Vertex() = default;
	Vertex(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {};
};

struct TexCoords
{
	float u, v;
	TexCoords() = default;
	TexCoords(float _u, float _v) : u(_u), v(_v) {};
};

struct Normals
{
	float nx, ny, nz;
	Normals() = default;
	Normals(float _nx, float _ny, float _nz) : nx(_nx), ny(_ny), nz(_nz) {};
};

struct Tangents
{
	float tx, ty, tz;
	Tangents() = default;
	Tangents(float _tx, float _ty, float _tz) : tx(_tx), ty(_ty), tz(_tz) {};
};

struct InterleavedData
{
	Vertex vertex;
	TexCoords texCoords;
	Normals normals;
	Tangents tangents;
	InterleavedData() = default;
	InterleavedData(Vertex _vertex,
		TexCoords _texCoords,
		Normals _normals,
		Tangents _tangents)
		: vertex(_vertex),
		texCoords(_texCoords),
		normals(_normals),
		tangents(_tangents) {
	};

};