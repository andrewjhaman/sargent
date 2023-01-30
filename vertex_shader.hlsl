//
//struct VertexInput 
//{
//	float3 in_position : POSITION;
//	float3 in_colour : COLOR;
//};

struct VertexOutput
{
	float3 colour : COLOR;
	float4 position : SV_Position;
};


struct DrawInfo
{
	uint vertex_buffer_index;
	float3 position;
	float4 quat;
};

cbuffer GlobalBindings : register(b0, space0)
{
	DrawInfo draw_info;
};


float3 rotate_vec_by_quat(float3 v, float4 q)
{
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}


struct Vertex
{
	float3 position;
	float3 normal;
};


#define BUFFER_SPACE space0
StructuredBuffer<Vertex> VertexBufferTable[] : register(t0, BUFFER_SPACE);


VertexOutput main(uint vertex_id : SV_VertexID)
{
	//float3 in_pos    = vertex_input.in_position;
	//float3 in_colour = vertex_input.in_colour;

	Vertex vertex = VertexBufferTable[draw_info.vertex_buffer_index].Load(vertex_id);

	float3 in_pos = vertex.position;
	float3 in_colour = vertex.normal;

	//
	//in_colour.y *= sin(time_other * 10) * 0.5 + 0.5;
	//float half_angle = time_other * 0.5;
	//
	//float4 quat = { 0.0, sin(half_angle), 0.0, cos(half_angle) };
	//quat = normalize(quat);
	//in_pos = rotate_vec_by_quat(in_pos, quat);
//
//
	in_pos += draw_info.position;
	//in_pos.z += 2.5;


	VertexOutput output;
	
	output.position.x = in_pos.x;
	output.position.y = in_pos.y * (1920/1080);
	output.position.z = in_pos.z - 0.2;
	output.position.w = in_pos.z;



	output.colour = in_colour;
	return output;
}