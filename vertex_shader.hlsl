
struct VertexInput 
{
	float3 in_position : POSITION;
	float3 in_colour : COLOR;
};

struct VertexOutput
{
	float3 colour : COLOR;
	float4 position : SV_Position;
};


cbuffer ObjectBindings : register(b0)
{
	float time_other;
	float3 position;
};

float3 rotate_vec_by_quat(float3 v, float4 q)
{
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}


VertexOutput main(VertexInput vertex_input)
{
   float3 in_colour = vertex_input.in_colour;
   float3 in_pos    = vertex_input.in_position;

   in_colour.y *= sin(time_other * 10) * 0.5 + 0.5;

   float half_angle = time_other * 0.5;

   float4 quat = { 0.0, sin(half_angle), 0.0, cos(half_angle) };
   quat = normalize(quat);
   in_pos = rotate_vec_by_quat(in_pos, quat);


   in_pos += position;


   VertexOutput output;

   output.position.x = in_pos.x;
   output.position.y = in_pos.y;
   output.position.z = in_pos.z - 0.2;
   output.position.w = in_pos.z;



   output.colour = in_colour;
   return output;
}