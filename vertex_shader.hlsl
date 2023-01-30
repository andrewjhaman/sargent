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
	float time;
};

//struct Globals
//{
//	float time;
//};

cbuffer PerDrawBindings : register(b0, space0)
{
	DrawInfo draw_info;
};


//cbuffer GlobalBindings : register(b1, space0)
//{
//	Globals globals;
//};


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
	//float3 in_pos    = vertex_input.in_position;
	//float3 in_colour = vertex_input.in_colour;

	Vertex vertex = VertexBufferTable[draw_info.vertex_buffer_index].Load(vertex_id);

	float3 in_pos = vertex.position;
	float3 in_colour = vertex.normal;


	float4 quat = { 0.0, 1.0, 0.0, cos(draw_info.time / 2) };
	quat = normalize(quat);

	quat = qmul(draw_info.quat, quat);
	quat = normalize(quat);

	in_pos = rotate_vec_by_quat(in_pos, quat);
	in_pos += draw_info.position;
	//in_pos.z += 20.5;

	float fov = 70.0 * 3.14158 / 180.0;
	float e = 1 / tan(fov * 0.5);
	float a = 1920 / 1080;
	float n = 0.01;


	float4x4 perspective = { e,   0,  0,  0,
							 0, e*a,  0,  0,
							 0,   0,  -1-0.001,  -2*n,
							 0,   0,  -1,  0 };

	VertexOutput output;
	
	//output.position = perspective * float4(in_pos, 1.0);
	output.position = mul( perspective, float4(in_pos, 1.0));
	//output.position.x = in_pos.x * e;
	//output.position.y = in_pos.y * e * a;
	//output.position.z = in_pos.z - 2*n;
	//output.position.w = in_pos.z;

	//output.position.y = in_pos.y * (1920/1080);
//	output.position.z = in_pos.z - 0.2;
//	output.position.w = in_pos.z;



	output.colour = in_colour;
	return output;
}