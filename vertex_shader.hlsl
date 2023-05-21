
struct VertexOutput
{
	float4 position : SV_Position;
	float3 colour : COLOR;
	float3 normal : NORMAL;
};


struct DrawInfo
{
	float4 quat;
	float3 position;
	uint vertex_buffer_index;
	uint __packing_a;
	uint __packing_b;
	uint __packing_c;
};

struct Globals
{
	row_major float4x4 projection;
	row_major float4x4 view;
	float time;
};

cbuffer PerDrawBindings : register(b0, space0)
{
	DrawInfo draw_info;
};


cbuffer GlobalBindings : register(b1, space0)
{
	Globals globals;
};


float3 rotate_vec_by_quat(float3 v, float4 q)
{
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
// Quaternion multiplication
// http://mathworld.wolfram.com/Quaternion.html
float4 qmul(float4 q1, float4 q2)
{
	return float4(
		q2.xyz * q1.w + q1.xyz * q2.w + cross(q1.xyz, q2.xyz),
		q1.w * q2.w - dot(q1.xyz, q2.xyz)
	);
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
	Vertex vertex = VertexBufferTable[draw_info.vertex_buffer_index].Load(vertex_id);

	float3 in_pos = vertex.position;
	float3 in_colour = vertex.normal;

	float4 quat = { 0.0, 1.0, 0.0, cos(globals.time / 2) };
	quat = normalize(quat);

	quat = qmul(draw_info.quat, quat);
	quat = normalize(quat);

	in_pos = rotate_vec_by_quat(in_pos, quat);
	in_pos += draw_info.position;

	VertexOutput output;

	output.position = mul( globals.projection, mul(globals.view, float4(in_pos, 1.0)));

	output.colour = in_colour;
	output.normal = normalize(rotate_vec_by_quat(vertex.normal, quat));

	return output;
}