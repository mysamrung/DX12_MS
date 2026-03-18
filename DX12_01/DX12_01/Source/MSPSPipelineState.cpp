#include "MSPSPipelineState.h"
#include "Engine.h"
#include <d3dx12.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

MSPSPipelineState::MSPSPipelineState()
{
	desc = {};
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // ラスタライザーはデフォルト
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングはなし
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // ブレンドステートもデフォルト
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // 深度ステンシルはデフォルトを使う
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形を描画
	desc.NumRenderTargets = 1; // 描画対象は1
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1; // サンプラーは1
	desc.SampleDesc.Quality = 0;
}

bool MSPSPipelineState::IsValid()
{
	return m_IsValid;
}

void MSPSPipelineState::SetAS(std::wstring filePath)
{
	auto hr = D3DReadFileToBlob(filePath.c_str(), m_pAsBlob.GetAddressOf());
	if (FAILED(hr))
	{
		printf("ピクセルシェーダーの読み込みに失敗");
		return;
	}


	g_Engine->Device()->CreateRootSignature(0, m_pAsBlob->GetBufferPointer(), m_pAsBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
	desc.pRootSignature = m_rootSignature.Get();

	desc.AS = CD3DX12_SHADER_BYTECODE(m_pAsBlob.Get());
}

void MSPSPipelineState::SetMS(std::wstring filePath)
{
	// 頂点シェーダー読み込み
	auto hr = D3DReadFileToBlob(filePath.c_str(), m_pMsBlob.GetAddressOf());
	if (FAILED(hr))
	{
		printf("頂点シェーダーの読み込みに失敗");
		return;
	}
	desc.MS = CD3DX12_SHADER_BYTECODE(m_pMsBlob.Get());
}

void MSPSPipelineState::SetPS(std::wstring filePath)
{
	// ピクセルシェーダー読み込み
	auto hr = D3DReadFileToBlob(filePath.c_str(), m_pPSBlob.GetAddressOf());
	if (FAILED(hr))
	{
		printf("ピクセルシェーダーの読み込みに失敗");
		return;
	}

	desc.PS = CD3DX12_SHADER_BYTECODE(m_pPSBlob.Get());
}

void MSPSPipelineState::Create()
{

	auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(desc);

	D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
	streamDesc.pPipelineStateSubobjectStream = &psoStream;
	streamDesc.SizeInBytes = sizeof(psoStream);

	// パイプラインステートを生成
	auto hr = g_Engine->Device()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(m_pPipelineState.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		printf("パイプラインステートの生成に失敗");
		return;
	}

	m_IsValid = true;
}

ID3D12RootSignature* MSPSPipelineState::GetRootSignature()
{
	return m_rootSignature.Get();
}

ID3D12PipelineState* MSPSPipelineState::Get()
{
	return m_pPipelineState.Get();
}
