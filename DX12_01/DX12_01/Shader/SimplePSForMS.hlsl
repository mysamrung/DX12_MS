
//SamplerState smp; // サンプラー
//Texture2D _MainTex : register(t4); // テクスチャ


struct VSOutput
{
    float4 PositionHS   : SV_Position; 
    float3 PositionVS   : POSITION0; 
    float3 Normal       : NORMAL0; 
    float2 UV           : COLOR0; 
};

float4 main(VSOutput input) : SV_TARGET
{
return float4(input.UV,0, 1);
	//return _MainTex.Sample(smp, input.UV);
}