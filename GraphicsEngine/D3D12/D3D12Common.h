#pragma once
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

#include <Windows.h>
#include <Psapi.h>
#include <wrl.h>
#include <comdef.h>
#include <shellapi.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>
#include <dxcapi.h>

#ifndef DEEP_D3D12_DEBUG_MODE
#define DEEP_D3D12_DEBUG_MODE 0
#endif