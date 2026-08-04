#pragma once

#include "PathingTrails.h"
#include "WorldGpsMath.h"

#include <cstdint>
#include <vector>

#include <d3d11.h>
#include <windows.h>

/* Shared D3D11 device/shader state for WorldGpsD3dDevice / WorldGpsD3dDraw. */
namespace WorldGpsD3dInternal
{
	struct Vertex
	{
		float px, py, pz;
		float u, v;
		float r, g, b, a;
	};

	struct alignas(16) Constants
	{
		float viewProj[16];
		float avatar[4];
		float camPos[4];
		float fade[4];
	};

	extern ID3D11Device*            gDev;
	extern ID3D11DeviceContext*     gCtx;
	extern ID3D11VertexShader*      gVS;
	extern ID3D11PixelShader*       gPS;
	extern ID3D11PixelShader*       gPSTex;
	extern ID3D11InputLayout*       gLayout;
	extern ID3D11Buffer*            gVB;
	extern ID3D11Buffer*            gCB;
	extern ID3D11BlendState*        gBlend;
	extern ID3D11RasterizerState*   gRaster;
	extern ID3D11DepthStencilState* gDepth;
	extern ID3D11SamplerState*      gSamp;
	extern UINT                     gVBCapacity;
	extern bool                     gHardFail;
	extern bool                     gOk;
	extern int                      gShaderRev;
	extern HMODULE                  gCompiler;

	constexpr int kShaderRev = 6; /* bump when embedded HLSL changes */

	void ReleaseGpu();
	bool EnsureDevice();
	bool EnsureVB(UINT vertexCount);
}
