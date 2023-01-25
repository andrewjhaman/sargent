
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
	float3x4 vertex_transform;
	float3x4 object_to_world;
	float3x4 inverse_transpose_object_to_world;
};


VertexOutput main(VertexInput vertex_input)
{
   float3 in_colour = vertex_input.in_colour;
   float3 in_pos    = vertex_input.in_position;

   in_colour.y *= sin(time_other * 10) * 0.5 + 0.5;

   VertexOutput output;
   output.position = float4(in_pos, 1.0);
   output.colour = in_colour;
   return output;
}