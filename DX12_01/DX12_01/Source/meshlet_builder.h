#pragma once
#include <d3dx12.h>
#include <DirectXMath.h>
#include "meshoptimizer.h"

struct Vertex;
struct meshopt_Meshlet;

struct Meshlet
{
    uint32_t vertexOffset;
    uint32_t vertexCount;

    uint32_t triangleOffset;
    uint32_t triangleCount;

    DirectX::XMFLOAT3 center;
    float radius;

    DirectX::XMFLOAT3 coneAxis;
    float coneCutoff;
};

struct MeshletData
{
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint8_t> meshletTriangles;
    std::vector<Meshlet> meshletsInfo;
};

struct Subset
{
    uint32_t Offset;
    uint32_t Count;
};

class MeshletModel
{
public:
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> VertexResources;
    Microsoft::WRL::ComPtr<ID3D12Resource>              IndexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              MeshletResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              UniqueVertexIndexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              PrimitiveIndexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              CullDataResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              MeshInfoResource;

    // D3D resource references
    std::vector<D3D12_VERTEX_BUFFER_VIEW>  VBViews;
    D3D12_INDEX_BUFFER_VIEW                IBView;

    MeshletData data;
};

struct MeshInfo
{
    uint32_t IndexSize;
    uint32_t MeshletCount;

    uint32_t LastMeshletVertCount;
    uint32_t LastMeshletPrimCount;
};

MeshletModel build_meshlets(
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices);
