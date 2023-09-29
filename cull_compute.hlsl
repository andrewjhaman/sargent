


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
	float bounding_radius;	
	int pa;
	int pb;
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

struct Globals
{
	row_major float4x4 projection;
	row_major float4x4 view;
	float time;
};
cbuffer GlobalBindings : register(b2, space0)
{
	Globals globals;
};




[numthreads(64, 1, 1)]
void main (uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	uint input_index = dispatch_thread_id.x;

	uint num_draw_calls;
	uint draw_call_stride;

	input_draw_calls.GetDimensions(num_draw_calls, draw_call_stride);

    if (input_index >= num_draw_calls)
        return;

	DrawCallInfo draw_call_info = input_draw_calls[input_index];

	float4x4 mat = globals.projection;

	float4 row_1 = mat[0];
	float4 row_2 = mat[1];
	float4 row_3 = mat[2];
	float4 row_4 = mat[3];

	float4 frustum_planes[6];
	frustum_planes[0] = row_4+row_1;
	frustum_planes[1] = row_4-row_1;
	frustum_planes[2] = row_4+row_2;
	frustum_planes[3] = row_4-row_2;
	frustum_planes[4] = row_4;
	frustum_planes[5] = row_4+row_3;

	float4 p = float4(draw_call_info.draw_info.position.x, draw_call_info.draw_info.position.y, draw_call_info.draw_info.position.z, 1.0);
	p = mul(globals.view,p);

	bool is_visible = true;

	bool inside = true;
	for(int i = 0; i < 5; ++i) {
		if (dot(p, frustum_planes[i]) < -draw_call_info.bounding_radius) {
			inside = false;
			break;
		}
	}
	is_visible = is_visible && inside;

	if(!is_visible) {
		return;
	}


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