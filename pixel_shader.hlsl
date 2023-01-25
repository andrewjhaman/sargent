
struct PixelInput 
{
    float3 colour : COLOR;
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

cbuffer GlobalBindings : register(b0)
{
	float time;
};

PixelOutput main(PixelInput pixel_input)
{
	float3 in_colour = pixel_input.colour;
    in_colour = saturate(pixel_input.colour);
                                   
	in_colour.x *= sin(time) * 0.5 + 0.5;
                                    
    PixelOutput output;
    output.attachment0 = float4(in_colour, 1.0);
    return output;
}
                                
                            