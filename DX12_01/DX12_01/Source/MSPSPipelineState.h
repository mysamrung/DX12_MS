#pragma once
#include "ComPtr.h"
#include <d3dx12.h>
#include <string>

class MSPSPipelineState
{
public:
	MSPSPipelineState(); // コンストラクタである程度の設定をする
	bool IsValid(); // 生成に成功したかどうかを返す

	void SetAS(std::wstring filePath); // ピクセルシェーダーを設定
	void SetMS(std::wstring filePath); // ピクセルシェーダーを設定
	void SetPS(std::wstring filePath); // ピクセルシェーダーを設定
	void Create(); // パイプラインステートを生成

	ID3D12RootSignature* GetRootSignature();
	ID3D12PipelineState* Get();

private:
	bool m_IsValid = false; // 生成に成功したかどうか
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC desc = {}; // パイプラインステートの設定
	ComPtr<ID3D12PipelineState> m_pPipelineState = nullptr; // パイプラインステート
	ComPtr<ID3DBlob> m_pAsBlob; // 頂点シェーダー
	ComPtr<ID3DBlob> m_pMsBlob; // 頂点シェーダー
	ComPtr<ID3DBlob> m_pPSBlob; // ピクセルシェーダー
	ComPtr<ID3D12RootSignature> m_rootSignature;
};
