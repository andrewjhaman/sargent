


struct D3D12_INDEX_BUFFER_VIEW {
	double BufferLocation;//uint64
	uint SizeInBytes;
	uint Format;//DXGI_FORMAT
};

struct DrawInfo {
	float4 quat;
	float3 position;
	uint vertex_buffer_index;
	uint __packing_a;
	uint __packing_b;
	uint __packing_c;
	uint __packing_d;
};


//@Speed: Most likely, seperating the vertex buffer and index buffer stuff from
//         the cull info stuff will lead to a speed up by reducing memory pressure?
struct DrawCallInfo {
	DrawInfo draw_info;
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;
	uint triangle_count;
	int pa;
	int pb;
	int pc;
	//float bounding_radius;	
};


struct D3D12_DRAW_INDEXED_ARGUMENTS {
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

struct DrawArguments {
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;
	DrawInfo draw_info;
	D3D12_DRAW_INDEXED_ARGUMENTS indexed;
	int packing_a;
	int packing_b;
};

#define BUFFER_SPACE space0
StructuredBuffer<DrawCallInfo> input_draw_calls : register(t0, BUFFER_SPACE);
AppendStructuredBuffer<DrawArguments> output_argument_buffer : register(u0, BUFFER_SPACE);

[numthreads(64, 1, 1)]
void main (uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	uint input_index = dispatch_thread_id.x;

	uint num_draw_calls;
	uint draw_call_stride;

	input_draw_calls.GetDimensions(num_draw_calls, draw_call_stride);

    if (input_index >= num_draw_calls)
        return;

	//for now, just pass everything through
	DrawCallInfo draw_call_info = input_draw_calls[input_index];

	DrawArguments result;

	{
		result.draw_info.__packing_a = 0;
		result.draw_info.__packing_b = 0;
		result.draw_info.__packing_c = 0;
		result.draw_info.__packing_d = 0;
		result.packing_a = 0;
		result.packing_b = 0;
	}

	result.index_buffer_view = draw_call_info.index_buffer_view;
	result.draw_info = draw_call_info.draw_info;
	result.indexed.IndexCountPerInstance = draw_call_info.triangle_count * 3;
	result.indexed.InstanceCount = 1;
	result.indexed.StartIndexLocation = 0;
	result.indexed.BaseVertexLocation = 0;
	result.indexed.StartInstanceLocation = 0;



	output_argument_buffer.Append(result);
}