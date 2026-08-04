#include "WorldGpsD3d.h"
#include "WorldGpsD3dInternal.h"

#include "Globals.h"

#include <cstring>

#include <d3dcompiler.h>
#include <dxgi.h>

using Fn_D3DCompile = pD3DCompile;
using namespace WorldGpsD3dInternal;

namespace WorldGpsD3dInternal
{
	ID3D11Device*            gDev = nullptr;
	ID3D11DeviceContext*     gCtx = nullptr;
	ID3D11VertexShader*      gVS = nullptr;
	ID3D11PixelShader*       gPS = nullptr;
	ID3D11PixelShader*       gPSTex = nullptr;
	ID3D11InputLayout*       gLayout = nullptr;
	ID3D11Buffer*            gVB = nullptr;
	ID3D11Buffer*            gCB = nullptr;
	ID3D11BlendState*        gBlend = nullptr;
	ID3D11RasterizerState*   gRaster = nullptr;
	ID3D11DepthStencilState* gDepth = nullptr;
	ID3D11SamplerState*      gSamp = nullptr;
	UINT                     gVBCapacity = 0;
	bool                     gHardFail = false;
	bool                     gOk = false;
	int                      gShaderRev = 0;
	HMODULE                  gCompiler = nullptr;
}

namespace
{
	const char* kHlsl = R"(
cbuffer Frame : register(b0)
{
	float4x4 gViewProj;
	float4   gAvatar;
	float4   gCamPos;
	float4   gFade;
};

struct VSIn
{
	float3 pos : POSITION;
	float2 uv  : TEXCOORD0;
	float4 col : COLOR0;
};

struct PSIn
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0;
	float4 col : COLOR0;
	float3 wpos : TEXCOORD1;
};

PSIn VSMain(VSIn i)
{
	PSIn o;
	o.pos = mul(gViewProj, float4(i.pos, 1));
	/* gCamPos.w = Blish-style animspeed flow scroll along the chevron. */
	o.uv = float2(i.uv.x, i.uv.y + gCamPos.w);
	o.col = i.col;
	o.wpos = i.pos;
	return o;
}

float SoftClear(float3 wpos)
{
	float clearMul = gAvatar.w;
	if (clearMul < 0.001) return 1;
	float3 d = wpos - gAvatar.xyz;
	float h = length(float2(d.x, d.z));
	float v = abs(d.y);
	if (v >= 3.5) return 1;
	float hideM = gFade.z;
	float fadeM = gFade.w;
	if (h <= hideM) return 0;
	if (h < fadeM) return saturate((h - hideM) / max(0.25, fadeM - hideM));
	return 1;
}

float RangeFade(float3 wpos)
{
	float3 d = wpos - gAvatar.xyz;
	float dist = length(float3(d.x, d.y * 0.5, d.z));
	float fadeStart = gFade.x;
	float fadeEnd = gFade.y;
	if (dist > fadeEnd) return 0;
	if (dist > fadeStart)
		return 1 - saturate((dist - fadeStart) / max(1, fadeEnd - fadeStart));
	return 1;
}

float4 PSSolid(PSIn i) : SV_Target
{
	/* Procedural chevron when pack texture is missing — matches Blish density. */
	float stripe = frac(i.uv.y);
	float chev = saturate(smoothstep(0.0, 0.12, stripe) * smoothstep(0.95, 0.45, stripe));
	float a = i.col.a * chev * SoftClear(i.wpos) * RangeFade(i.wpos);
	if (a < 0.02) discard;
	return float4(i.col.rgb, a);
}

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

float4 PSTextured(PSIn i) : SV_Target
{
	/* Blish: tint × trail texture (alpha shapes the chevron). */
	float4 t = gTex.Sample(gSamp, i.uv);
	float a = t.a * i.col.a * SoftClear(i.wpos) * RangeFade(i.wpos);
	if (a < 0.015) discard;
	return float4(t.rgb * i.col.rgb, a);
}
)";

	Fn_D3DCompile LoadCompiler()
	{
		if (!gCompiler)
		{
			static const wchar_t* kNames[] = {
				L"d3dcompiler_47.dll",
				L"d3dcompiler_46.dll",
				L"d3dcompiler_43.dll",
			};
			for (const wchar_t* n : kNames)
			{
				gCompiler = LoadLibraryW(n);
				if (gCompiler)
					break;
			}
		}
		if (!gCompiler)
			return nullptr;
		FARPROC proc = GetProcAddress(gCompiler, "D3DCompile");
		Fn_D3DCompile fn = nullptr;
		std::memcpy(&fn, &proc, sizeof(fn));
		return fn;
	}

	bool Compile(Fn_D3DCompile compile, const char* entry, const char* profile,
		ID3DBlob** outBlob)
	{
		ID3DBlob* err = nullptr;
		const HRESULT hr = compile(
			kHlsl, static_cast<SIZE_T>(std::strlen(kHlsl)), "WorldGpsD3d.hlsl",
			nullptr, nullptr, entry, profile, 0, 0, outBlob, &err);
		if (err)
			err->Release();
		return SUCCEEDED(hr) && outBlob && *outBlob;
	}
}

void WorldGpsD3dInternal::ReleaseGpu()
{
	if (gSamp) { gSamp->Release(); gSamp = nullptr; }
	if (gDepth) { gDepth->Release(); gDepth = nullptr; }
	if (gRaster) { gRaster->Release(); gRaster = nullptr; }
	if (gBlend) { gBlend->Release(); gBlend = nullptr; }
	if (gCB) { gCB->Release(); gCB = nullptr; }
	if (gVB) { gVB->Release(); gVB = nullptr; }
	if (gLayout) { gLayout->Release(); gLayout = nullptr; }
	if (gPSTex) { gPSTex->Release(); gPSTex = nullptr; }
	if (gPS) { gPS->Release(); gPS = nullptr; }
	if (gVS) { gVS->Release(); gVS = nullptr; }
	if (gCtx) { gCtx->Release(); gCtx = nullptr; }
	if (gDev) { gDev->Release(); gDev = nullptr; }
	gVBCapacity = 0;
	gOk = false;
	gShaderRev = 0;
}

bool WorldGpsD3dInternal::EnsureDevice()
{
	if (gOk && gDev && gCtx && gShaderRev == kShaderRev)
		return true;
	if (gHardFail)
		return false;
	if (!G::API || !G::API->SwapChain)
		return false;

	ReleaseGpu();

	auto* swap = static_cast<IDXGISwapChain*>(G::API->SwapChain);
	ID3D11Device* dev = nullptr;
	if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&dev))) || !dev)
		return false;
	ID3D11DeviceContext* ctx = nullptr;
	dev->GetImmediateContext(&ctx);
	if (!ctx)
	{
		dev->Release();
		return false;
	}
	gDev = dev;
	gCtx = ctx;

	Fn_D3DCompile compile = LoadCompiler();
	if (!compile)
	{
		ReleaseGpu();
		gHardFail = true;
		return false;
	}

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* psTexBlob = nullptr;
	if (!Compile(compile, "VSMain", "vs_4_0", &vsBlob) ||
		!Compile(compile, "PSSolid", "ps_4_0", &psBlob) ||
		!Compile(compile, "PSTextured", "ps_4_0", &psTexBlob))
	{
		if (vsBlob) vsBlob->Release();
		if (psBlob) psBlob->Release();
		if (psTexBlob) psTexBlob->Release();
		ReleaseGpu();
		gHardFail = true;
		return false;
	}

	if (FAILED(gDev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			nullptr, &gVS)) ||
		FAILED(gDev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr, &gPS)) ||
		FAILED(gDev->CreatePixelShader(psTexBlob->GetBufferPointer(), psTexBlob->GetBufferSize(),
			nullptr, &gPSTex)))
	{
		vsBlob->Release();
		psBlob->Release();
		psTexBlob->Release();
		ReleaseGpu();
		gHardFail = true;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	const HRESULT layHr = gDev->CreateInputLayout(
		layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &gLayout);
	vsBlob->Release();
	psBlob->Release();
	psTexBlob->Release();
	if (FAILED(layHr) || !gLayout)
	{
		ReleaseGpu();
		gHardFail = true;
		return false;
	}

	D3D11_BUFFER_DESC cbd{};
	cbd.ByteWidth = sizeof(Constants);
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(gDev->CreateBuffer(&cbd, nullptr, &gCB)))
	{
		ReleaseGpu();
		gHardFail = true;
		return false;
	}

	D3D11_BLEND_DESC bd{};
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	gDev->CreateBlendState(&bd, &gBlend);

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	gDev->CreateRasterizerState(&rd, &gRaster);

	D3D11_DEPTH_STENCIL_DESC dd{};
	dd.DepthEnable = FALSE;
	dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dd.StencilEnable = FALSE;
	gDev->CreateDepthStencilState(&dd, &gDepth);

	D3D11_SAMPLER_DESC sd{};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	gDev->CreateSamplerState(&sd, &gSamp);

	gOk = gVS && gPS && gPSTex && gLayout && gCB && gBlend && gRaster && gDepth && gSamp;
	if (!gOk)
	{
		ReleaseGpu();
		gHardFail = true;
	}
	else
		gShaderRev = kShaderRev;
	return gOk;
}

bool WorldGpsD3d::Available()
{
	return EnsureDevice();
}

void WorldGpsD3d::Shutdown()
{
	ReleaseGpu();
	if (gCompiler)
	{
		FreeLibrary(gCompiler);
		gCompiler = nullptr;
	}
	gHardFail = false;
}
