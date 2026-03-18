#define ROOT_SIG "CBV(b0), \
                  CBV(b1),  \
                  SRV(t0), \
                  SRV(t1), \
                  SRV(t2), \
                  SRV(t3)"

struct Transform
{
    float4x4 World;
    float4x4 View;
    float4x4 Proj;
    float4 Planes[6];
    float4 CameraPos;
};

struct MeshInfo 
{
    uint IndexSize;
    uint MeshletCount;

    uint LastMeshletVertCount;
    uint LastMeshletPrimCount;
};

struct Vertex 
{ 
    float3 Position;
    float3 Normal;
    float2 UV;
    float3 Tangent;
    float4 Color;
}; 

struct VertexOut 
{ 
    float4 PositionHS   : SV_Position; 
    float3 PositionVS   : POSITION0; 
    float3 Normal       : NORMAL0; 
}; 

struct Meshlet 
{
    uint VertOffset;
    uint VertCount;

    uint PrimOffset;
    uint PrimCount;

    float3 center;
    float radius;

    float3 coneAxis;
    float coneCutoff;
};

struct Payload
{
    uint MeshletIndices[32];
};

ConstantBuffer<Transform> Globals             : register(b0);
ConstantBuffer<MeshInfo> MeshInfo             : register(b1);

StructuredBuffer<Vertex>  Vertices            : register(t0);
StructuredBuffer<Meshlet> Meshlets            : register(t1);
StructuredBuffer<uint>      UniqueVertexIndices : register(t2);
ByteAddressBuffer           PrimitiveIndices    : register(t3);

groupshared Payload s_Payload;

bool ConeVisible(Meshlet m)
{
    float3 view = normalize(Globals.CameraPos - m.center);

    float d = dot(view, m.coneAxis);

    return d >= m.coneCutoff;
}

bool IsVisible(Meshlet m, float4x4 world, float scale)
{
    float4 center = mul(float4(m.center.xyz, 1), world);
    float radius = m.radius * scale;

    for (int i = 0; i < 6; ++i)
    {
        if (dot(center, Globals.Planes[i]) < -radius)
        {
            return false;
        }
    }

    if(!ConeVisible(m))
        return false;

 
    return true;
}


[RootSignature(ROOT_SIG)]
[NumThreads(32, 1, 1)]
void main(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupID)
{
  bool visible = false;

    // Check bounds of meshlet cull data resource
    if (dtid < MeshInfo.MeshletCount)
    {
        // Do visibility testing for this thread
        visible = IsVisible(Meshlets[dtid], Globals.World, 1);
    }

    // Compact visible meshlets into the export payload array
    if (visible)
    {
        uint index = WavePrefixCountBits(visible);
        s_Payload.MeshletIndices[index] = dtid;
    }

    // Dispatch the required number of MS threadgroups to render the visible meshlets
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, s_Payload);
}
