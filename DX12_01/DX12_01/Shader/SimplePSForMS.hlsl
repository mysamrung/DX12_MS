struct VSOutput
{
    float4 PositionHS   : SV_Position; 
    float3 PositionVS   : POSITION0; 
    float3 Normal       : NORMAL0; 
};


//SamplerState smp : register(s0); // サンプラー
//Texture2D _MainTex : register(t0); // テクスチャ

float4 main(VSOutput input) : SV_TARGET
{
return 1;
	//return _MainTex.Sample(smp, input.uv);
}