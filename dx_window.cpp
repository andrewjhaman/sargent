#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

#include "SargentMath.h"

static u32 window_width = 1920;
static u32 window_height = 1080;

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <Windows.h>
#include <ShellScalingAPI.h>

#include <dbghelp.h>

#include <timeapi.h>

#include <assert.h>

#include <stdio.h>

//#include <D3DCompiler.h>
#include <dxcapi.h>
#include <d3d12shader.h>


#include <D3d12.h>
#include <D3d12SDKLayers.h>
#include <dxgi.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>


#include "utils.h"

//#define FAST_OBJ_IMPLEMENTATION
//#include "fast_obj.h"


#define OBJ_PARSE_IMPLEMENTATION
#include "obj_parse.h"

//http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
//The following line is to favor the high performance NVIDIA GPU if there are multiple GPUs
//Has to be .exe module to be correctly detected.
extern "C" { _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001; }
//And the AMD equivalent
//Also has to be .exe module to be correctly detected.
extern "C" { _declspec(dllexport) unsigned int AmdPowerXpressRequestHighPerformance = 0x00000001; }


#pragma comment( lib, "D3d12" )
#pragma comment( lib, "DXGI" )
#pragma comment( lib, "User32")
#pragma comment( lib, "dxcompiler")


#include <comdef.h>
bool _must_succeed(HRESULT result) {
	if (result < 0) {
		_com_error err(result);
		printf("COM Error (HRESULT %ld): %s\n", (long)result, err.ErrorMessage());
		return false;
	}
	return true;
}

#define REPORT_ERROR(message) do { char __message_buffer[4096*2]; snprintf(__message_buffer, sizeof(__message_buffer), "l:%d: %s", __LINE__, message); MessageBoxEx(0, __message_buffer, "Sargent Renderer Error!", 0, 0); ExitProcess(1); } while (0);
#define MUST_SUCCEED(result) assert(_must_succeed(result))


static constexpr u32 back_buffer_count = 2;


u64 triangle_count = 0;


ID3D12Device* device;


ID3D12DescriptorHeap* vertex_buffer_heap;

ID3D12CommandAllocator* command_allocator;
ID3D12GraphicsCommandList* command_list = 0;
ID3D12PipelineState* pipeline_state = 0;
ID3D12RootSignature* root_signature;
D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_handle = {};
IDXGISwapChain3* swap_chain;
D3D12_VIEWPORT viewport;
D3D12_RECT surface_rect;
u32 current_buffer;
ID3D12DescriptorHeap* render_target_view_heap;
ID3D12Resource* render_targets[back_buffer_count];
u32 render_target_view_descriptor_size;


D3D12_DEPTH_STENCIL_DESC default_depth_stencil_state;
ID3D12DescriptorHeap* depth_stencil_descriptor_heap;
ID3D12Resource* depth_stencil_targets[back_buffer_count];
u32 depth_stencil_target_descriptor_size;

ID3D12CommandQueue* command_queue;
UINT frame_index;
HANDLE fence_event;
ID3D12Fence* fence;
UINT64 fence_value;

struct alignas(16) DrawInfo
{
	u32 vertex_buffer_index;
	vec3 position;
	vec4 quat;
};

struct alignas(16) ShaderGlobals
{
	Mat4x4 projection;
	Mat4x4 view;
	float time;
};

struct Buffer
{
	size_t size_in_bytes;
	ID3D12Resource* resource;
};


Buffer vertex_buffer_1;
Buffer vertex_buffer_2;

//Buffer constant_buffer;


Buffer create_buffer(size_t size_in_bytes)
{
	Buffer result = {};

	result.size_in_bytes = size_in_bytes;

	D3D12_HEAP_PROPERTIES heap_properties = {};
	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 1;
	heap_properties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resource_description = {};
	resource_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_description.Alignment = 0;
	resource_description.Width = size_in_bytes;
	resource_description.Height = 1;
	resource_description.DepthOrArraySize = 1;
	resource_description.MipLevels = 1;
	resource_description.Format = DXGI_FORMAT_UNKNOWN;
	resource_description.SampleDesc.Count = 1;
	resource_description.SampleDesc.Quality = 0;
	resource_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_description.Flags = D3D12_RESOURCE_FLAG_NONE;

	MUST_SUCCEED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.resource)));

	return result;
}

void upload_to_buffer(Buffer* buffer, void* data, size_t data_size_in_bytes)
{
	D3D12_RANGE read_range = {};
	void* upload_destination = 0;

	assert(buffer->size_in_bytes >= data_size_in_bytes);

	MUST_SUCCEED(buffer->resource->Map(0, &read_range, (void**)&upload_destination));
	memcpy(upload_destination, data, data_size_in_bytes);
	buffer->resource->Unmap(0, nullptr);
}


struct Vertex
{
	f32 position[3];
	f32 normal [3];
};


struct Mesh
{
	u32 index_count;
	Buffer index_buffer;//We may not need to keep this around (We only use the index_buffer_view when rendering right now.)
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;

	u32 vertex_count;
	Buffer vertex_buffer;
};


Mesh meshes[4096];
u32 mesh_count;

Mesh load_obj(char* filename)
{
	Mesh result = {};

	ParsedOBJ obj = LoadOBJ(filename);
	assert(obj.renderable_count == 1);
	ParsedOBJRenderable renderable = obj.renderables[0];
	//Vertex* vertex_buffer_data = (Vertex*)renderable.vertices;
	Vertex* vertex_buffer_data = new Vertex[renderable.vertex_count];

	for (u32 i = 0; i < renderable.vertex_count; i++)
	{
		vertex_buffer_data[i].position[0] = renderable.vertices[i*8+0];
		vertex_buffer_data[i].position[1] = renderable.vertices[i*8+1];
		vertex_buffer_data[i].position[2] = renderable.vertices[i*8+2];

		vertex_buffer_data[i].normal[0] = renderable.vertices[i*8+3+2];
		vertex_buffer_data[i].normal[1] = renderable.vertices[i*8+4+2];
		vertex_buffer_data[i].normal[2] = renderable.vertices[i*8+5+2];
	}
	u32 vertex_buffer_size = sizeof(Vertex) * renderable.vertex_count;

	result.vertex_buffer = create_buffer(sizeof(Vertex)*renderable.vertex_count);
	result.vertex_count = renderable.vertex_count;
	upload_to_buffer(&result.vertex_buffer, vertex_buffer_data, vertex_buffer_size);

	result.index_count = renderable.index_count;
	u32* index_buffer_data = (u32*)renderable.indices;
	u32 index_buffer_size = sizeof(u32) * renderable.index_count;


	result.index_buffer = create_buffer(index_buffer_size);

	upload_to_buffer(&result.index_buffer, index_buffer_data, index_buffer_size);

	result.index_buffer_view.BufferLocation = result.index_buffer.resource->GetGPUVirtualAddress();
	result.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
	result.index_buffer_view.SizeInBytes = index_buffer_size;

	delete [] vertex_buffer_data;
	FreeParsedOBJ(&obj);

	return result;
}


void init_directx12(HWND window)
{
	ID3D12Debug1* debug_controller;
	IDXGIFactory2* factory;
	
	UINT dxgi_factory_flags = 0;
	
	HRESULT result = 0;
#if defined(_DEBUG)
	ID3D12Debug* dc;
	MUST_SUCCEED(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
	MUST_SUCCEED(dc->QueryInterface(IID_PPV_ARGS(&debug_controller)));
	debug_controller->EnableDebugLayer();
	debug_controller->SetEnableGPUBasedValidation(true);
	
	dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	
	MUST_SUCCEED(dc->Release());
	dc = nullptr;
#endif
	
	MUST_SUCCEED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));
	
	
#define DX_FEATURE_LEVEL (D3D_FEATURE_LEVEL_12_0)
	
	IDXGIAdapter1* adapter;

	size_t highest_dedicated_device_memory_size = 0;
	for(UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapter_index, &adapter); ++adapter_index)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		
		if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;
		
		if(highest_dedicated_device_memory_size < desc.DedicatedVideoMemory &&
			SUCCEEDED(D3D12CreateDevice(adapter, DX_FEATURE_LEVEL, _uuidof(ID3D12Device), nullptr)))
		{
			highest_dedicated_device_memory_size = desc.DedicatedVideoMemory;
			printf("Using device %ls.\n", desc.Description);
			// break;
		}
		else
			adapter->Release();
	}
	
	
	
	MUST_SUCCEED(D3D12CreateDevice(adapter, DX_FEATURE_LEVEL, IID_PPV_ARGS(&device)));
	
	
#if defined(_DEBUG)
	ID3D12DebugDevice* debug_device = 0;;
	ID3D12InfoQueue* debug_info_queue = 0;
	MUST_SUCCEED(device->QueryInterface(&debug_device));
	MUST_SUCCEED(device->QueryInterface(IID_PPV_ARGS(&debug_info_queue)));
	debug_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	debug_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	debug_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

	debug_info_queue->Release();
#endif
	
	
	
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
	
	MUST_SUCCEED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));
	
	MUST_SUCCEED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
	
	
	
	
	
	MUST_SUCCEED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
#if 0
	
	ID3D12GraphicsCommandList* command_list;
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = tex_resource;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_RESOURCES;
	
	MUST_SUCCEED(command_list->ResourceBarrier(1, &barrier));
#endif
	
	
	surface_rect.left = 0;
	surface_rect.top = 0;
	surface_rect.right  = (u64)window_width;
	surface_rect.bottom = (u64)window_height;
	
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width  = (u64)window_width;
	viewport.Height = (u64)window_height;
	viewport.MinDepth = 0.1f;
	viewport.MaxDepth = 1000.0f;
	
	
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.BufferCount = back_buffer_count;
	swap_chain_desc.Width = window_width;
	swap_chain_desc.Height = window_height;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.SampleDesc.Count = 1;
	
	IDXGISwapChain1* base_swap_chain;
	MUST_SUCCEED(factory->CreateSwapChainForHwnd(command_queue, window, &swap_chain_desc, NULL, NULL, &base_swap_chain));
	MUST_SUCCEED(base_swap_chain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swap_chain));
	base_swap_chain->Release();
	base_swap_chain = 0;
	
	frame_index = swap_chain->GetCurrentBackBufferIndex();
	
	
	D3D12_DESCRIPTOR_HEAP_DESC render_target_view_heap_description = {};
	render_target_view_heap_description.NumDescriptors = back_buffer_count;
	render_target_view_heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	render_target_view_heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	MUST_SUCCEED(device->CreateDescriptorHeap(&render_target_view_heap_description, IID_PPV_ARGS(&render_target_view_heap)));
	
	render_target_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	render_target_view_handle = {render_target_view_heap->GetCPUDescriptorHandleForHeapStart()};
	
	
	for(u32 i = 0; i < back_buffer_count; ++i)
	{
		MUST_SUCCEED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i], nullptr, render_target_view_handle);
		render_target_view_handle.ptr += (1 * render_target_view_descriptor_size);
	}
	

	//Depth Stencil
	{
		default_depth_stencil_state.DepthEnable = true;
		default_depth_stencil_state.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		default_depth_stencil_state.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		default_depth_stencil_state.StencilEnable = false;
		default_depth_stencil_state.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK; 
		default_depth_stencil_state.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		D3D12_DEPTH_STENCILOP_DESC stencil_op = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		default_depth_stencil_state.FrontFace = stencil_op;
		default_depth_stencil_state.BackFace = stencil_op;



		D3D12_DESCRIPTOR_HEAP_DESC depth_stencil_heap_descriptions = {};
		depth_stencil_heap_descriptions.NumDescriptors = back_buffer_count;
		depth_stencil_heap_descriptions.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		depth_stencil_heap_descriptions.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		MUST_SUCCEED(device->CreateDescriptorHeap(&depth_stencil_heap_descriptions, IID_PPV_ARGS(&depth_stencil_descriptor_heap)));

		D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_description = {};
		depth_stencil_view_description.Format = DXGI_FORMAT_D32_FLOAT;
		depth_stencil_view_description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_description.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
		depth_optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		depth_optimized_clear_value.DepthStencil.Depth = 0.0f;
		depth_optimized_clear_value.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_properties.CreationNodeMask = 1;
		heap_properties.VisibleNodeMask = 1;


		D3D12_RESOURCE_DESC resource_description = {};
		resource_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resource_description.Alignment = 0;
		resource_description.Width = window_width;
		resource_description.Height = window_height;
		resource_description.DepthOrArraySize = 1;
		resource_description.MipLevels = 1;
		resource_description.Format = DXGI_FORMAT_D32_FLOAT;
		resource_description.SampleDesc.Count = 1;
		resource_description.SampleDesc.Quality = 0;
		resource_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resource_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;


		D3D12_CPU_DESCRIPTOR_HANDLE handle = depth_stencil_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		depth_stencil_target_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		for (int i = 0; i < back_buffer_count; ++i)
		{
			device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_optimized_clear_value,IID_PPV_ARGS(&depth_stencil_targets[i]));
			device->CreateDepthStencilView(depth_stencil_targets[i], &depth_stencil_view_description, handle);
			handle.ptr += depth_stencil_target_descriptor_size;
		}
		depth_stencil_descriptor_heap->SetName(L"Depth/Stencil Resource Heap");

	}
	
	
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	
	if(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)) != S_OK)
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	

	//Check support for mesh shaders
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature_data_2;
	device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &feature_data_2, sizeof(feature_data_2));
	printf("Mesh shaders are%s supported.\n", feature_data_2.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED ? " not" : "");
	
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].RegisterSpace = 0;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = UINT_MAX;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;


	D3D12_ROOT_PARAMETER1 root_parameters[3];
	root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[0].DescriptorTable.NumDescriptorRanges = sizeof(ranges) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	root_parameters[0].DescriptorTable.pDescriptorRanges = ranges;

	root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[1].Constants.ShaderRegister = 0;
	root_parameters[1].Constants.RegisterSpace  = 0;
	root_parameters[1].Constants.Num32BitValues = (sizeof(DrawInfo) + 3) / 4;

	
	root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[2].Constants.ShaderRegister = 1;
	root_parameters[2].Constants.RegisterSpace  = 0;
	root_parameters[2].Constants.Num32BitValues = (sizeof(ShaderGlobals) + 3) / 4;
	


	
	
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
	root_signature_description.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	root_signature_description.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		/*D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | 
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;*/
	root_signature_description.Desc_1_1.NumParameters = sizeof(root_parameters) / sizeof(D3D12_ROOT_PARAMETER1);
	root_signature_description.Desc_1_1.pParameters = root_parameters;
	root_signature_description.Desc_1_1.NumStaticSamplers = 0;
	root_signature_description.Desc_1_1.pStaticSamplers = 0;
	
	ID3DBlob* signature;
	ID3DBlob* error;
	
	if(D3D12SerializeVersionedRootSignature(&root_signature_description, &signature, &error) != S_OK)
	{
		char* error_string = (char*)error->GetBufferPointer();
			REPORT_ERROR(error_string);
		error->Release();
		error = 0;
	}
	if(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)) != S_OK)
	{
		char* error_string = (char*)error->GetBufferPointer();
		REPORT_ERROR(error_string);
		error->Release();
		error = 0;
	}
	
	if(signature)
	{
		signature->Release();
		signature = 0;
	}
	
	meshes[mesh_count++] = load_obj("bunny.obj");
	meshes[mesh_count++] = load_obj("Apollo_Statue.obj");
	
	// Vertex Buffer SRV here.
	{
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.NumDescriptors = mesh_count;
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		
		MUST_SUCCEED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&vertex_buffer_heap)));

		D3D12_SHADER_RESOURCE_VIEW_DESC vertex_srv_description = {};
		vertex_srv_description.Format = DXGI_FORMAT_UNKNOWN;
		vertex_srv_description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		vertex_srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		vertex_srv_description.Buffer.FirstElement = 0;
		vertex_srv_description.Buffer.NumElements = 0;
		vertex_srv_description.Buffer.StructureByteStride = sizeof(Vertex);


		D3D12_CPU_DESCRIPTOR_HANDLE handle = vertex_buffer_heap->GetCPUDescriptorHandleForHeapStart();

		//for (Mesh& mesh : meshes)
		for(int i = 0; i < mesh_count; ++i)
		{
			Mesh& mesh = meshes[i];
			vertex_srv_description.Buffer.NumElements = mesh.vertex_count;
			device->CreateShaderResourceView(mesh.vertex_buffer.resource, &vertex_srv_description, handle);
			handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}


#if 0
	constant_buffer = create_buffer(1024*1024);
	{
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.NumDescriptors = 1;
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		MUST_SUCCEED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&constant_buffer_heap)));

		D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc = {};
		constant_buffer_view_desc.BufferLocation = constant_buffer.resource->GetGPUVirtualAddress();
		constant_buffer_view_desc.SizeInBytes = (sizeof(ShaderGlobals) + 255) & ~255;	// CB size is required to be 256-byte aligned.
		device->CreateConstantBufferView(&constant_buffer_view_desc, constant_buffer_heap->GetCPUDescriptorHandleForHeapStart());
	}
#endif

	D3D12_SHADER_BYTECODE vertex_shader_byte_code;
	D3D12_SHADER_BYTECODE pixel_shader_byte_code;
	
	ID3DBlob* errors = 0;
	u32 compile_flags = 0;

	IDxcUtils* utils;
	IDxcCompiler3* compiler;
	IDxcIncludeHandler* include_handler;

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	utils->CreateDefaultIncludeHandler(&include_handler);

	LPCWSTR vertex_shader_path = L"vertex_shader.hlsl";
	LPCWSTR vertex_shader_args[] =
	{
		vertex_shader_path,
		L"-E", L"main",
		L"-T", L"vs_6_5",
		L"-Zi"
	};
	IDxcBlobEncoding* vertex_source_pointer = 0;
	utils->LoadFile(vertex_shader_path, 0, &vertex_source_pointer);
	DxcBuffer vertex_source;
	vertex_source.Ptr  = vertex_source_pointer->GetBufferPointer();
	vertex_source.Size = vertex_source_pointer->GetBufferSize();
	vertex_source.Encoding = DXC_CP_ACP;

	IDxcResult* vertex_shader_results;
	compiler->Compile(&vertex_source, vertex_shader_args, sizeof(vertex_shader_args) / sizeof(vertex_shader_args[0]), include_handler, IID_PPV_ARGS(&vertex_shader_results));

	IDxcBlobUtf8* shader_errors = 0;
	vertex_shader_results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shader_errors), 0);

	if(shader_errors && shader_errors->GetStringLength()) printf("Shader Compilation Errors:\n%ls:%s\n", vertex_shader_path, (char*)shader_errors->GetBufferPointer());

	HRESULT vertex_shader_status;
	vertex_shader_results->GetStatus(&vertex_shader_status);
	if(FAILED(vertex_shader_status)) REPORT_ERROR("Vertex Shader Blob is not valid.!");

	IDxcBlob* vertex_shader_blob = 0;
	IDxcBlobUtf16* vertex_shader_name = 0;
	vertex_shader_results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&vertex_shader_blob), &vertex_shader_name);

	vertex_shader_byte_code.pShaderBytecode = vertex_shader_blob->GetBufferPointer();
	vertex_shader_byte_code.BytecodeLength = vertex_shader_blob->GetBufferSize();
	if(!vertex_shader_byte_code.pShaderBytecode || !vertex_shader_byte_code.BytecodeLength)
		REPORT_ERROR("Error getting bytecode from vertex shader!");

	
	LPCWSTR pixel_shader_path = L"pixel_shader.hlsl";
	LPCWSTR pixel_shader_args[] =
	{
		vertex_shader_path,
		L"-E", L"main",
		L"-T", L"ps_6_5",
		L"-Zi"
	};
	IDxcBlobEncoding* pixel_source_pointer = 0;
	utils->LoadFile(pixel_shader_path, 0, &pixel_source_pointer);
	DxcBuffer pixel_source;
	pixel_source.Ptr  = pixel_source_pointer->GetBufferPointer();
	pixel_source.Size = pixel_source_pointer->GetBufferSize();
	pixel_source.Encoding = DXC_CP_ACP;

	IDxcResult* pixel_shader_results;
	compiler->Compile(&pixel_source, pixel_shader_args, sizeof(pixel_shader_args) / sizeof(pixel_shader_args[0]), include_handler, IID_PPV_ARGS(&pixel_shader_results));

	pixel_shader_results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shader_errors), 0);

	if(shader_errors && shader_errors->GetStringLength()) printf("Shader Compilation Errors:\n%ls:%s\n", pixel_shader_path, (char*)shader_errors->GetBufferPointer());

	HRESULT pixel_shader_status;
	pixel_shader_results->GetStatus(&pixel_shader_status);
	if(FAILED(pixel_shader_status)) REPORT_ERROR("Pixel Shader Blob is not valid.!");

	IDxcBlob* pixel_shader_blob = 0;
	IDxcBlobUtf16* pixel_shader_name = 0;
	pixel_shader_results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pixel_shader_blob), &pixel_shader_name);

	pixel_shader_byte_code.pShaderBytecode = pixel_shader_blob->GetBufferPointer();
	pixel_shader_byte_code.BytecodeLength = pixel_shader_blob->GetBufferSize();
	if(!pixel_shader_byte_code.pShaderBytecode || !pixel_shader_byte_code.BytecodeLength)
		REPORT_ERROR("Error getting bytecode from pixel shader!");
	
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_object_description = {};
   
	
	pipeline_state_object_description.pRootSignature = root_signature;
	
	
	D3D12_RASTERIZER_DESC rasterizer_description;
	rasterizer_description.FillMode = D3D12_FILL_MODE_SOLID;
	//rasterizer_description.CullMode = D3D12_CULL_MODE_NONE;
	rasterizer_description.CullMode = D3D12_CULL_MODE_BACK;
	rasterizer_description.FrontCounterClockwise = TRUE;
	rasterizer_description.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer_description.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_description.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer_description.DepthClipEnable = TRUE;
	rasterizer_description.MultisampleEnable = FALSE;
	rasterizer_description.AntialiasedLineEnable = FALSE;
	rasterizer_description.ForcedSampleCount = 0;
	rasterizer_description.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipeline_state_object_description.RasterizerState = rasterizer_description;
	pipeline_state_object_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	
	D3D12_BLEND_DESC blend_description;
	blend_description.AlphaToCoverageEnable  = FALSE;
	blend_description.IndependentBlendEnable = FALSE;
	
	D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_description = {
		FALSE,
		FALSE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	
	for (u32 i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		blend_description.RenderTarget[i] = default_render_target_blend_description;
	
	
	pipeline_state_object_description.VS = vertex_shader_byte_code;
	pipeline_state_object_description.PS = pixel_shader_byte_code;
	
	pipeline_state_object_description.BlendState = blend_description;
	

	pipeline_state_object_description.DepthStencilState = default_depth_stencil_state;;
	pipeline_state_object_description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipeline_state_object_description.SampleMask = UINT_MAX;
	
	pipeline_state_object_description.NumRenderTargets = 1;
	pipeline_state_object_description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipeline_state_object_description.SampleDesc.Count = 1;
	
	MUST_SUCCEED(device->CreateGraphicsPipelineState(&pipeline_state_object_description, IID_PPV_ARGS(&pipeline_state)));
	
	ID3D12PipelineState* initial_pipeline_state = 0;
	
	MUST_SUCCEED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator, initial_pipeline_state, IID_PPV_ARGS(&command_list)));
	MUST_SUCCEED(command_list->Close());

	pixel_shader_blob->Release();
	vertex_shader_blob->Release();
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	
	switch(message)
	{
		default:
		{
			result = DefWindowProc(window, message, wParam, lParam);
		}break;
	}
	return result;
}

void draw(f64 dt)
{
	MUST_SUCCEED(command_allocator->Reset());
	MUST_SUCCEED(command_list->Reset(command_allocator, 0));

	command_list->SetPipelineState(pipeline_state);
	
	
	D3D12_RESOURCE_BARRIER render_target_barrier;
	render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	render_target_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	render_target_barrier.Transition.pResource = render_targets[frame_index];
	render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	render_target_barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	render_target_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	command_list->ResourceBarrier(1, &render_target_barrier);



	command_list->SetGraphicsRootSignature(root_signature);
	
	

	ID3D12DescriptorHeap* descriptor_heaps[] = { vertex_buffer_heap };
	command_list->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);
	command_list->SetGraphicsRootDescriptorTable(0, vertex_buffer_heap->GetGPUDescriptorHandleForHeapStart());

	render_target_view_handle = {render_target_view_heap->GetCPUDescriptorHandleForHeapStart()};
	render_target_view_handle.ptr += frame_index * render_target_view_descriptor_size;
	
	D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_target_view_handle = { depth_stencil_descriptor_heap->GetCPUDescriptorHandleForHeapStart() };
	depth_stencil_target_view_handle.ptr += frame_index * depth_stencil_target_descriptor_size;

	command_list->OMSetRenderTargets(1, &render_target_view_handle, FALSE, &depth_stencil_target_view_handle);
	
	
	f32 clear_colour[]  {0.03f, 0.19f, 0.22f, 1.0f};
	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &surface_rect);
	command_list->ClearRenderTargetView(render_target_view_handle, clear_colour, 0, nullptr);
	command_list->ClearDepthStencilView(depth_stencil_target_view_handle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	static f64 time = 0.0f;
	time += dt;

	rng = 101;
	advance_rng((&rng));

	u32 draw_count = 50;
	

	ShaderGlobals global_data = {};

	f32 fov = 70.0 * 3.14159265 / 180.0;
	f32 e = 1.0 / tanf(fov * 0.5f);
	f32 a = f32(window_width) / f32(window_height);
	f32 n = 0.01f;

	global_data.projection = perspective_infinite_reversed_z(70.0f, 0.01f, (f32)window_width, (f32)window_height);

	
	vec3 cam_pos = {};
	cam_pos.z -= time;

	vec3 cam_dir = Vec3(sinf((f32)time), 0.0f, -2.0f);
	cam_dir = normalize(cam_dir);
	//cam_dir = { 0.0f, 0.0f, -1.0f };
	
	vec3 target = cam_pos + cam_dir;
	global_data.view = look_at(cam_pos, target, { 0.0f, 1.0f, 0.0f });
	//global_data.view = transpose(global_data.view);
	global_data.time = (f32)time;

	command_list->SetGraphicsRoot32BitConstants(2, (sizeof(global_data) + 3) / 4, &global_data, 0);

	srand(101);

	triangle_count = 0;

	f32 p_range = 5.0f;
	for (u32 i = 0; i < draw_count; ++i)
	{	
		u32 mesh_index = rand() % mesh_count;
		Mesh& mesh = meshes[mesh_index];
		command_list->IASetIndexBuffer(&mesh.index_buffer_view);
		DrawInfo draw_info = {};
		draw_info.position = { rand_f32_in_range(-p_range, p_range, &rng), rand_f32_in_range(-p_range, p_range, &rng), -10.0f + rand_f32_in_range(-p_range, p_range, &rng) };
		draw_info.quat = { rand_f32_in_range(-1.0, 1.0, &rng), rand_f32_in_range(-1.0, 1.0, &rng), rand_f32_in_range(-1.0, 1.0, &rng), rand_f32_in_range(-1.0, 1.0, &rng) };
		draw_info.quat = normalize(draw_info.quat);
		//draw_info.quat = { 0.0f, 0.0f, 0.0f, 1.0f };

		draw_info.position.z += 2.5f;
		draw_info.vertex_buffer_index = mesh_index;

		command_list->SetGraphicsRoot32BitConstants(1, (sizeof(DrawInfo) + 3) / 4, &draw_info, 0);

		command_list->DrawIndexedInstanced(mesh.index_count, 1, 0, 0, 0);

		triangle_count += mesh.index_count / 3;
	}

	D3D12_RESOURCE_BARRIER present_barrier;
	present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	present_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	present_barrier.Transition.pResource = render_targets[frame_index];
	present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	present_barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
	present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	command_list->ResourceBarrier(1, &present_barrier);
	
	MUST_SUCCEED(command_list->Close());
	
		
	ID3D12CommandList* command_lists[] = {command_list};
	command_queue->ExecuteCommandLists(1, command_lists);

	swap_chain->Present(1, 0);
	
	
	++fence_value;
	u64 current_fence_value = fence_value;
	MUST_SUCCEED(command_queue->Signal(fence, current_fence_value));
	if(fence->GetCompletedValue() < current_fence_value)
	{
		MUST_SUCCEED(fence->SetEventOnCompletion(current_fence_value, fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}

	command_queue->Wait(fence, current_fence_value);


	frame_index = swap_chain->GetCurrentBackBufferIndex();
	
};



#if 0
struct Shader {

};

struct Mesh 
{
	f32 x;
};

Mesh* meshes[1024];
constexpr u32 mesh_count = sizeof(meshes) / sizeof(Mesh*);



struct BindGroup {

};

BindGroup* material_bindings[1024];
constexpr u32 material_count = sizeof(material_bindings) / sizeof(BindGroup*);

struct Draw {
	Shader* shader;
	Mesh*   mesh;
	BindGroup* bind_group;
	u32 dynamic_buffer_offset;
};


struct DrawArea {
	Viewport viewport;
	Scissor scissor;
	BindGroup* bind_group;
	BindGroup* bind_group_dynamic_offset_buffer;
	u32 starting_draw_index = 0;//Index in the array of Draws where should we start to use this DrawArea
	u32 draws_count = 1;// How many elements of the Draws array use this DrawArea
};

struct Matrix3x4 {
	f32 data[12];
};

Matrix3x4 translate(f32 x, f32 y, f32 z) {
	Matrix3x4 result = {};

	result.data[0+0] = x;
	result.data[4+1] = y;
	result.data[8+2] = z;

	return result;
};

struct DrawUniforms {
	float time;
	float3x4 vertex_transform;
	float3x4 object_to_world;
	float3x4 inverse_transpose_object_to_world;
};

#include <time.h>
#include <stdlib.h>
#include <vector>

u32 random_int(u32 upper_limit_exclusive) {
	return rand() % upper_limit_exclusive;
}


enum class RendererBackend {
	DX12 = 0,
	//VULKAN = 1,
	//METAL = 2,
};



struct Viewport {
	vec3 origin;
	vec3 size;

	constexpr vec3 SIZE_WHEN_FULL_SIZE = { INFINITY, INFINITY, INFINITY };
};

struct Scissor {
	
};

struct DXShader {
	ID3D12PipelineState* pipeline_state;
};


struct CommandBuffer {

};

struct RenderPassRenderer {
	vec3 dimensions = { 1920, 1080, 1 };
};

void draw_subpass(RenderPassRenderer* pass_renderer, DrawArea* draw_areas, u32 draw_area_count, Draw* draws, u32 draw_count)
{
	Shader* current_shader = 0;
	Mesh* current_mesh = 0;
	Viewport current_viewport = {};
	Scissor current_scissor = {};
	
	for (int i = 0; i < draw_area_count; ++i) {
		DrawArea* draw_area = draw_areas[i];

		//Viewport
		if (draw_area->viewport != current_viewport) {
			current_viewport = draw_area.viewport;
			D3D12_VIEWPORT viewport;
			viewport.TopLeftX = current_viewport.origin.x;
			viewport.TopLeftY = current_viewport.origin.y;

			vec3 size = current_viewport.size == Viewport::SIZE_WHEN_FULL_SIZE ? 
				(vec3)((float)pass_renderer->dimensions.x, (float)pass_renderer->dimensions.y, (float)pass_renderer->dimensions.z) : current_viewport.size;
			viewport.MinDepth = current_viewport.origin.z;
			viewport.MaxDepth = current_viewport.origin.z;
			command_list->RSSetViewports(1, &viewport);
		}

		//Scissor
		if (draw_area->scissor != current_scissor) {
			current_scissor = draw_area->scissor;
			D3D12_RECT surface_rect;
			surface_rect.left = current_scissor.left;
			surface_rect.top = current_scissor.top;
			surface_rect.right  = current_scissor.right;
			surface_rect.bottom = current_viewport.bottom;
			command_list->RSSetScissorRects(1, &surface_rect);
		}

		//TODO(Andrew): BindGroup stuff

		
		for (int j = 0; j < draw_area->draws_count; ++j) {
			Draw* draw = draws[draw_area->starting_draw_index + j];

			if (draw->shader != current_shader) {
				current_shader = draw->shader;
				DXShader* shader = get_shader(somethingsomething, current_shader);
				assert(shader);

				command_list->SetPipelineState(shader->pipeline_state);
			}

			if (draw->mesh != current_mesh) {
				current_mesh = draw->mesh;
				DXMesh* mesh = get_mesh(somethingsomething, current_mesh);

				command_list->IASetIndexBuffer(mesh->index_buffer_view);

				MeshInfo info = mesh->info;
				D3D12_VERTEX_BUFFER_VIEW buffer_views[info.num_vertex_buffers];

				for (int k = 0; k < info.num_vertex_buffers, ++k) {
					DXBuffer* buffer = get_buffer(somethingsomething, info.vertex_buffers[k]);
					assert(buffer);
					buffer_views[buffer_view_count] = buffer->view;
				}

				command_list->IASetVertexBuffers(0, info.num_vertex_buffers, &buffer_views);
			}
			command_list->DrawIndexedInstanced(info.index_count, draw.instance_count, info.index_offset, info.vertex_offset, 0);
		}
		
	}
}

void game_render() {
	//RenderFrame frame = renderer_begin_frame();



	//Main Pass
	{
		CommandBuffer* command_buffer = renderer_begin_commands();
		RenderPassRenderer* pass_renderer = begin_render_pass(&command_buffer, frame.main_render_pass, frame.frame_buffer);

		constexpr u32 object_count = 10000;
		auto draws = std::vector<Draw>(object_count);


		u32 data_offset = 0;
		for (int i = 0; i , object_count; ++i)
		{
			u32 material_index = random_int(material_count);
			u32 mesh_index = random_int(mesh_count);

			Draw draw;
			//draw.shader = shader,
			draw.mesh = meshes[mesh_index],
			draw.bind_group = material_bindings[material_index];
			draw.dynamic_buffer_offset = data_offset;

			draws.push_back(draw);

			//bindgroup = 
			f32 x = rand_f32_in_range(-50.0f, 50.0f, &rng);
			f32 y = rand_f32_in_range(-50.0f, 50.0f, &rng);
			f32 z = rand_f32_in_range(-50.0f, 50.0f, &rng);
			Matrix3x4 mat = translate_3x4(x, y, z);
			bindgroup->model_matrix = mat;
			bindgroup->model_inverse_transpose = inverse_3x3(to_3x3(mat));
			//data_offset = align_up(data_offset + (u32)sizeof(DrawUniforms), alignment.uniform_offset);
			data_offset += (u32)sizeof(DrawUniforms);

		}


		DrawArea draw_area;
		draw_area.bind_group = global_bindings;
		draw_area.bind_group_dynamic_offset_buffer = draw_bindings;
		draw_area.starting_draw_index = 0;
		draw_area.draws_count = object_count;

		draw_subpass(&pass_renderer, &draw_area, 1, draws.data(), draws.size);

		end_render_pass(&command_buffer, &pass_renderer);
		submit_commands(&command_buffer);
	}

}
#endif


WINDOWPLACEMENT previous_window_placement = {};
void win32_toggle_fullscreen(HWND window)
{
	constexpr bool allow_fullscreen_optimisations = true;
    
	DWORD style = GetWindowLong(window, GWL_STYLE);
	if (style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO monitorInfo = {sizeof(monitorInfo)};
		if(GetWindowPlacement(window, &previous_window_placement) &&
			GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
		{
            
			//We set the style this way in order to make sure Fullscreen Optimisations get enabled. 
			//style = 0x96000000;
			if(allow_fullscreen_optimisations)
				style = WS_VISIBLE | WS_POPUP;
			else
				style = WS_VISIBLE;
			SetWindowLong(window, GWL_STYLE, style);
            
			SetWindowPos(window, HWND_NOTOPMOST,
			             monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			             monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			             monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			             SWP_SHOWWINDOW);
		}
	}
	else
	{
		SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW | WS_POPUP);
		SetWindowPlacement(window, &previous_window_placement);
		SetWindowPos(window, NULL, 0, 0, 0, 0,
		             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOCOPYBITS);
	}
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdArgs, int intCmdShow)
{
#if 1
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr); 
	
	HWND console_window = GetConsoleWindow();
#endif
	
	LPCSTR window_class_name = "DirectX12";
	LPCSTR window_title = "DirectX12 Viewer";
	
	WNDCLASSEX window_class = {};
	
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = windowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance;
	window_class.hIcon = NULL;
	window_class.hIconSm = NULL;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = window_class_name;
	
   
	RegisterClassEx(&window_class);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	RECT work_area;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);
	window_width = work_area.right - work_area.left;
	window_height = work_area.bottom - work_area.top;
	
	window_width /= 2;
	window_height /= 2;

	HWND window = CreateWindowEx(0, window_class_name, window_title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, 0, 0, hInstance, 0);
	//if(!window)
	//REPORT_ERROR("Call to CreateWindowEx Failed!");
	
	
	init_directx12(window);

	MSG message = {};

	LARGE_INTEGER timer_frequency;
	QueryPerformanceFrequency(&timer_frequency);

	LARGE_INTEGER last_count;
	QueryPerformanceCounter(&last_count);

	f64 smooth_dt = 1 / 60.0f;

	bool should_quit = false;
	while(!should_quit)
	{
		LARGE_INTEGER current_count;
		QueryPerformanceCounter(&current_count);
		f64 dt = f64(current_count.QuadPart - last_count.QuadPart) / (f64)timer_frequency.QuadPart;
		last_count = current_count;

		f64 percent = 0.1;
		smooth_dt = dt*percent + dt*(1.0-percent);

		while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
		{
			switch (message.message)
			{
				case WM_PAINT:
				{
					PAINTSTRUCT paintStruct;
					HDC deviceContext = BeginPaint(window, &paintStruct);
					draw(dt);

					//win32DrawWindow(deviceContext);
					EndPaint(window, &paintStruct);
				}break;
				case WM_KEYDOWN:
				{
					if (message.wParam == VK_ESCAPE)
						should_quit = true;
					if (message.wParam == VK_F11)
						win32_toggle_fullscreen(window);
				}break;
				case WM_CLOSE:
				case WM_DESTROY:
				case WM_QUIT:
				{
					should_quit = true;
					//DestroyWindow(console_window);
					//PostQuitMessage(0);
					//ExitProcess(0);
				}break;
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		
		draw(dt);

		char window_title[4096];
		sprintf_s(window_title, sizeof(window_title), "Sargent Renderer: dt:%.2fms, Tris:%llu, %.3fM:tris/s", smooth_dt*1000, triangle_count, f64(triangle_count)/smooth_dt/1000000);
		SetWindowTextA(window, window_title);
	}
}