
struct PixelInput 
{
	float4 position : SV_Position;
    float3 colour : COLOR;
	float3 normal : NORMAL;
};

struct PixelOutput
{
    float4 attachment0 : SV_Target0;
};



//cbuffer MaterialBindings : register(b3)
//{
//	float roughness;
//	float metalness;
//};

//cbuffer GlobalBindings : register(b0)
//{
//	float time;
//};
struct Globals
{
	row_major float4x4 projection;
	row_major float4x4 view;
	float time;
};
cbuffer GlobalBindings : register(b1, space0)
{
	Globals globals;
};



PixelOutput main(PixelInput pixel_input)
{
	//float3 in_colour = pixel_input.colour;
    //in_colour = saturate(pixel_input.colour);
	float3 sun_direction = { sin(-globals.time*5), -1.0f, cos(-globals.time*5) };
	sun_direction = normalize(-sun_direction);
	float3 in_colour = dot(sun_direction, pixel_input.normal);
                                   
	//in_colour.x *= sin(time) * 0.5 + 0.5;
                                    
    PixelOutput output;
    output.attachment0 = float4(in_colour, 1.0);
    return output;
}
                                
                            