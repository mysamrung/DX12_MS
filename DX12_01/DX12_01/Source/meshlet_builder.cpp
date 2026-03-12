#include "meshlet_builder.h"
#include "meshoptimizer.h"
#include "Engine.h"

#include <stdexcept>
#include "SharedStruct.h"

template <typename T, typename U>
constexpr T DivRoundUp(T num, U denom)
{
    return (num + denom - 1) / denom;
}

inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}
MeshletModel build_meshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    const size_t max_vertices = 64;
    const size_t max_triangles = 126;

    size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

    MeshletModel model;
    
    model.data.meshlets.resize(max_meshlets);
    model.data.meshletVertices.resize(max_meshlets * max_vertices);
    model.data.meshletTriangles.resize(max_meshlets * max_triangles * 3);
    size_t meshletCount = meshopt_buildMeshlets(
        model.data.meshlets.data(),
        model.data.meshletVertices.data(),
        model.data.meshletTriangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].Position.x,
        vertices.size(),
        sizeof(Vertex),
        max_vertices,
        max_triangles,
        0.0f);

    model.data.meshlets.resize(meshletCount);
    model.data.meshletVertices.resize(meshletCount * max_vertices);
    model.data.meshletTriangles.resize(meshletCount * max_triangles * 3);


    for (size_t i = 0; i < meshletCount; i++)
    {
        meshopt_Meshlet& m = model.data.meshlets[i];

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &model.data.meshletVertices[m.vertex_offset],
            &model.data.meshletTriangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].Position.x,
            vertices.size(),
            sizeof(Vertex));

        Meshlet out;

        out.vertexOffset = m.vertex_offset;
        out.vertexCount = m.vertex_count;
        out.triangleOffset = m.triangle_offset;
        out.triangleCount = m.triangle_count;

        //out.center = { bounds.center[0],bounds.center[1],bounds.center[2] };
        //out.radius = bounds.radius;


        //out.coneAxis = { bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2] };
        //out.coneCutoff = bounds.cone_cutoff;

        model.data.meshletsInfo.push_back(out);

    }


    // Create Buffer
    // Create committed D3D resources of proper sizes
    auto indexDesc = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(uint32_t));
    auto meshletDesc = CD3DX12_RESOURCE_DESC::Buffer(model.data.meshletsInfo.size() * sizeof(Meshlet));
    auto vertexIndexDesc = CD3DX12_RESOURCE_DESC::Buffer(model.data.meshletVertices.size() * sizeof(uint32_t));
    auto primitiveDesc = CD3DX12_RESOURCE_DESC::Buffer(model.data.meshletTriangles.size() * sizeof(uint8_t));
    auto meshInfoDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(MeshInfo));

    HRESULT result;

    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &indexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.IndexResource)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &meshletDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.MeshletResource)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vertexIndexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.UniqueVertexIndexResource)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &primitiveDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.PrimitiveIndexResource)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &meshInfoDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.MeshInfoResource)));
    

    // Create Resource Reference
    model.IBView.BufferLocation = model.IndexResource->GetGPUVirtualAddress();
    model.IBView.Format = DXGI_FORMAT_R32_UINT;
    model.IBView.SizeInBytes = indices.size() * sizeof(uint32_t);

    model.VertexResources.resize(1);
    model.VBViews.resize(1);

    for (uint32_t j = 0; j < model.VertexResources.size(); ++j)
    {
        auto vertexDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(Vertex));
        ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vertexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&model.VertexResources[j])));

        model.VBViews[j].BufferLocation = model.VertexResources[j]->GetGPUVirtualAddress();
        model.VBViews[j].SizeInBytes = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
        model.VBViews[j].StrideInBytes = sizeof(Vertex);
    }

    
    // Create upload resources
    std::vector<ComPtr<ID3D12Resource>> vertexUploads;
    ComPtr<ID3D12Resource>              indexUpload;
    ComPtr<ID3D12Resource>              meshletUpload;
    ComPtr<ID3D12Resource>              cullDataUpload;
    ComPtr<ID3D12Resource>              uniqueVertexIndexUpload;
    ComPtr<ID3D12Resource>              primitiveIndexUpload;
    ComPtr<ID3D12Resource>              meshInfoUpload;

    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &indexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexUpload)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &meshletDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshletUpload)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexIndexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uniqueVertexIndexUpload)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &primitiveDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&primitiveIndexUpload)));
    ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &meshInfoDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshInfoUpload)));

    // Map & copy memory to upload heap
    vertexUploads.resize(1);
    for (uint32_t j = 0; j < vertexUploads.size(); ++j)
    {
        auto vertexDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(Vertex));
        ThrowIfFailed(g_Engine->Device()->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexUploads[j])));

        uint8_t* memory = nullptr;
        vertexUploads[j]->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, vertices.data(), vertices.size() * sizeof(Vertex));
        vertexUploads[j]->Unmap(0, nullptr);
    }

    {
        uint8_t* memory = nullptr;
        indexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, indices.data(), indices.size() * sizeof(uint32_t));
        indexUpload->Unmap(0, nullptr);
    }

    {
        uint8_t* memory = nullptr;
        meshletUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, model.data.meshletsInfo.data(), model.data.meshletsInfo.size() * sizeof(Meshlet));
        meshletUpload->Unmap(0, nullptr);
    }

    {
        uint8_t* memory = nullptr;
        uniqueVertexIndexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, model.data.meshletVertices.data(), model.data.meshletVertices.size() * sizeof(uint32_t));
        uniqueVertexIndexUpload->Unmap(0, nullptr);
    }

    {
        uint8_t* memory = nullptr;
        primitiveIndexUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, model.data.meshletTriangles.data(), model.data.meshletTriangles.size() * sizeof(uint8_t));
        primitiveIndexUpload->Unmap(0, nullptr);
    }

    {
        MeshInfo info = {};
        info.IndexSize = 4;
        info.MeshletCount = model.data.meshletsInfo.size();
        info.LastMeshletVertCount = model.data.meshletsInfo.back().vertexCount;
        info.LastMeshletPrimCount = model.data.meshletsInfo.back().triangleCount;


        uint8_t* memory = nullptr;
        meshInfoUpload->Map(0, nullptr, reinterpret_cast<void**>(&memory));
        std::memcpy(memory, &info, sizeof(MeshInfo));
        meshInfoUpload->Unmap(0, nullptr);
    }

    // Populate our command list
    ThrowIfFailed(g_Engine->CommandList()->Reset(g_Engine->CommandAllocator(), nullptr));

    for (uint32_t j = 0; j < 1; ++j)
    {
        g_Engine->CommandList()->CopyResource(model.VertexResources[j].Get(), vertexUploads[j].Get());
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(model.VertexResources[j].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        g_Engine->CommandList()->ResourceBarrier(1, &barrier);
    }

    D3D12_RESOURCE_BARRIER postCopyBarriers[5];

    g_Engine->CommandList()->CopyResource(model.IndexResource.Get(), indexUpload.Get());
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(model.IndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    g_Engine->CommandList()->CopyResource(model.MeshletResource.Get(), meshletUpload.Get());
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(model.MeshletResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    g_Engine->CommandList()->CopyResource(model.UniqueVertexIndexResource.Get(), uniqueVertexIndexUpload.Get());
    postCopyBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(model.UniqueVertexIndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    g_Engine->CommandList()->CopyResource(model.PrimitiveIndexResource.Get(), primitiveIndexUpload.Get());
    postCopyBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(model.PrimitiveIndexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    g_Engine->CommandList()->CopyResource(model.MeshInfoResource.Get(), meshInfoUpload.Get());
    postCopyBarriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(model.MeshInfoResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    g_Engine->CommandList()->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

    ThrowIfFailed(g_Engine->CommandList()->Close());

    ID3D12CommandList* ppCommandLists[] = { g_Engine->CommandList() };
    g_Engine->CommandQueue()->ExecuteCommandLists(1, ppCommandLists);

    // Create our sync fence
    ComPtr<ID3D12Fence> fence;
    ThrowIfFailed(g_Engine->Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    g_Engine->CommandQueue()->Signal(fence.Get(), 1);

    // Wait for GPU
    if (fence->GetCompletedValue() != 1)
    {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence->SetEventOnCompletion(1, event);

        WaitForSingleObjectEx(event, INFINITE, false);
        CloseHandle(event);
    }

    return model;
}