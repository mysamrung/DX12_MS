#define ROOT_SIG "CBV(b0), \
                 CBV(b1), \
                 SRV(t0), \
                 SRV(t1), \
                 SRV(t2), \
                 SRV(t3), \
                 SRV(t4)"
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
    float2 UV           : COLOR0; 
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
    float coneCutOff;
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


/////
// Data Loaders
uint LoadByte(ByteAddressBuffer buf, uint address)
{
    uint aligned = address & ~3;
    uint shift = (address & 3) * 8;

    uint word = buf.Load(aligned);
    return (word >> shift) & 0xff;
}
uint3 GetPrimitive(Meshlet m, uint index)
{
    uint base = (m.PrimOffset) + index * 3;

    uint i0 = LoadByte(PrimitiveIndices, base + 0);
    uint i1 = LoadByte(PrimitiveIndices, base + 1);
    uint i2 = LoadByte(PrimitiveIndices, base + 2);

    return uint3(i0, i1, i2);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{

    return UniqueVertexIndices[m.VertOffset + localIndex];
}

VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex)
{
    Vertex v = Vertices[vertexIndex];
    
	float4 localPos = float4(v.Position, 1.0f);
	float4 worldPos = mul(Globals.World, localPos);
	float4 viewPos = mul(Globals.View, worldPos);
	float4 projPos = mul(Globals.Proj, viewPos);

    VertexOut vout;
    vout.PositionVS = worldPos;
    vout.PositionHS = projPos;
    float3 normalWS = normalize(mul((float3x3)Globals.World, v.Normal));
    vout.Normal = normalWS;
    vout.UV = v.UV;

    return vout;
}


[RootSignature(ROOT_SIG)]
[NumThreads(64, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint dtid : SV_DispatchThreadID,
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload Payload payload,
    out vertices VertexOut verts[64],
    out indices uint3 tris[126]
)
{
    uint meshletIndex = payload.MeshletIndices[gid];

    Meshlet m = Meshlets[meshletIndex];

    SetMeshOutputCounts(m.VertCount, m.PrimCount);
    
    if (gtid < m.PrimCount)
    {
        tris[gtid] = GetPrimitive(m, gtid);
    }

    if (gtid < m.VertCount)
    {
        uint vertexIndex = GetVertexIndex(m, gtid);
        verts[gtid] = GetVertexAttributes(gid, vertexIndex);
    }
}
