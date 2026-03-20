struct VSOutput
{
	float4 svpos : SV_POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD; // 頂点シェーダーから来たuv
};


SamplerState smp : register(s0); // サンプラー
Texture2D _MainTex : register(t4); // テクスチャ

float4 pixel(VSOutput input) : SV_TARGET
{
return 0.5;
	//return _MainTex.Sample(smp, input.uv);
}