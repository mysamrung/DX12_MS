#include "Scene.h"
#include "Engine.h"
#include "App.h"
#include <d3dx12.h>
#include "SharedStruct.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"
#include "VSPSPipelineState.h"
#include "MSPSPipelineState.h"
#include "AssimpLoader.h"
#include "DescriptorHeap.h"
#include "Texture2D.h"
#include <filesystem>
#include "meshlet_builder.h"

using namespace DirectX;
namespace fs = std::filesystem;

Scene* g_Scene;

ConstantBuffer* constantBuffer[Engine::FRAME_BUFFER_COUNT];
RootSignature* rootSignature;
VSPSPipelineState* vspsPipelineState;
MSPSPipelineState* mspsPipelineState;


const wchar_t* modelFile = L"Assets/Alicia/FBX/Alicia_solid_Unity.FBX";
std::vector<Mesh> meshes;

std::vector<VertexBuffer*> vertexBuffers; // メッシュの数分の頂点バッファ
std::vector<IndexBuffer*> indexBuffers; // メッシュの数分のインデックスバッファ

DescriptorHeap* descriptorHeap;
std::vector<DescriptorHandle*> materialHandles; // テクスチャ用のハンドル一覧

std::vector<MeshletModel> meshletsModel;


std::wstring ReplaceExtension(const std::wstring& origin, const char* ext)
{
	fs::path p = origin.c_str();
	return p.replace_extension(ext).c_str();
}

bool Scene::Init()
{
	// Constant Buffer
	auto eyePos = XMVectorSet(0.0f, 120.0, 75.0, 0.0f);
	auto targetPos = XMVectorSet(0.0f, 120.0, 0.0, 0.0f);
	auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // 上方向を表すベクトル
	auto fov = XMConvertToRadians(60.0f); // 視野角
	auto aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT); // アスペクト比

	for (size_t i = 0; i < Engine::FRAME_BUFFER_COUNT; i++)
	{
		constantBuffer[i] = new ConstantBuffer(sizeof(Transform));
		if (!constantBuffer[i]->IsValid())
		{
			printf("変換行列用定数バッファの生成に失敗\n");
			return false;
		}

		// 変換行列の登録
		auto ptr = constantBuffer[i]->GetPtr<Transform>();
		ptr->World = XMMatrixIdentity();
		ptr->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
		ptr->Proj = XMMatrixPerspectiveFovRH(fov, aspect, 0.3f, 1000.0f);
	}

	// Root Signature
	rootSignature = new RootSignature();
	if (!rootSignature->IsValid())
	{
		printf("ルートシグネチャの生成に失敗\n");
		return false;
	}

	// Pipeline State
	vspsPipelineState = new VSPSPipelineState();
	vspsPipelineState->SetInputLayout(Vertex::InputLayout);
	vspsPipelineState->SetRootSignature(rootSignature->Get());
	vspsPipelineState->SetVS(L"../x64/Debug/SimpleVS.cso");
	vspsPipelineState->SetPS(L"../x64/Debug/SimplePS.cso");
	vspsPipelineState->Create();
	if (!vspsPipelineState->IsValid())
	{
		printf("パイプラインステートの生成に失敗\n");
		return false;
	}


	mspsPipelineState = new MSPSPipelineState();
	mspsPipelineState->SetAS(L"../x64/Debug/SimpleAS.cso");
	mspsPipelineState->SetMS(L"../x64/Debug/SimpleMS.cso");
	mspsPipelineState->SetPS(L"../x64/Debug/SimplePSForMs.cso");
	mspsPipelineState->Create();
	if (!mspsPipelineState->IsValid())
	{
		printf("パイプラインステートの生成に失敗\n");
		return false;
	}

	/// Load Model Data
	ImportSettings importSetting = // これ自体は自作の読み込み設定構造体
	{
		modelFile,
		meshes,
		false,
		true // アリシアのモデルは、テクスチャのUVのVだけ反転してるっぽい？ので読み込み時にUV座標を逆転させる
	};

	AssimpLoader loader;
	if (!loader.Load(importSetting))
	{
		return false;
	}

	// Create Vertex Buffer
	vertexBuffers.reserve(meshes.size());
	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto size = sizeof(Vertex) * meshes[i].Vertices.size();
		auto stride = sizeof(Vertex);
		auto vertices = meshes[i].Vertices.data();
		auto pVB = new VertexBuffer(size, stride, vertices);
		if (!pVB->IsValid())
		{
			printf("頂点バッファの生成に失敗\n");
			return false;
		}

		vertexBuffers.push_back(pVB);
	}

	// Create Index Buffer
	indexBuffers.reserve(meshes.size());
	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto size = sizeof(uint32_t) * meshes[i].Indices.size();
		auto indices = meshes[i].Indices.data();
		auto pIB = new IndexBuffer(size, indices);
		if (!pIB->IsValid())
		{
			printf("インデックスバッファの生成に失敗\n");
			return false;
		}

		indexBuffers.push_back(pIB);
	}

	// Load Texture and create DescriptorHeap
	descriptorHeap = new DescriptorHeap();
	materialHandles.clear();
	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto texPath = ReplaceExtension(meshes[i].DiffuseMap, "tga"); // もともとはpsdになっていてちょっとめんどかったので、同梱されているtgaを読み込む
		auto mainTex = Texture2D::Get(texPath);
		auto handle = descriptorHeap->Register(mainTex);
		materialHandles.push_back(handle);
	}

	// Create Meshlet
	
	for (size_t i = 0; i < meshes.size(); i++)
	{
		meshletsModel.push_back(build_meshlets(meshes[i].Vertices, meshes[i].Indices));
	}

	printf("シーンの初期化に成功\n");
	return true;
}

void Scene::Update()
{
}

void Scene::Draw()
{
	auto currentIndex = g_Engine->CurrentBackBufferIndex();
	auto commandList = g_Engine->CommandList();
	auto materialHeap = descriptorHeap->GetHeap(); // ディスクリプタヒープ

	commandList->SetGraphicsRootSignature(mspsPipelineState->GetRootSignature());
	commandList->SetPipelineState(mspsPipelineState->Get());
	commandList->SetGraphicsRootConstantBufferView(0, constantBuffer[currentIndex]->GetAddress());

	for (auto& meshlet : meshletsModel)
	{
		commandList->SetGraphicsRootConstantBufferView(1, meshlet.MeshInfoResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(2, meshlet.VertexResources[0]->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(3, meshlet.MeshletResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(4, meshlet.UniqueVertexIndexResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootShaderResourceView(5, meshlet.PrimitiveIndexResource->GetGPUVirtualAddress());

		commandList->DispatchMesh(meshlet.data.meshlets.size(), 1, 1);
	}

//	// メッシュの数だけインデックス分の描画を行う処理を回す
//	for (size_t i = 0; i < meshes.size(); i++)
//	{
//		auto vbView = vertexBuffers[i]->View(); // そのメッシュに対応する頂点バッファ
//		auto ibView = indexBuffers[i]->View(); // そのメッシュに対応する頂点バッファ
//
//		commandList->SetGraphicsRootSignature(rootSignature->Get());
//		commandList->SetPipelineState(vspsPipelineState->Get());
//		commandList->SetGraphicsRootConstantBufferView(0, constantBuffer[currentIndex]->GetAddress());
//
//		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//		commandList->IASetVertexBuffers(0, 1, &vbView);
//		commandList->IASetIndexBuffer(&ibView);
//
//		commandList->SetDescriptorHeaps(1, &materialHeap); // 使用するディスクリプタヒープをセット
//		commandList->SetGraphicsRootDescriptorTable(1, materialHandles[i]->HandleGPU); // そのメッシュに対応するディスクリプタテーブルをセット
//
//		commandList->DrawIndexedInstanced(meshes[i].Indices.size(), 1, 0, 0, 0); // インデックスの数分描画する
//		break;
//	}
}