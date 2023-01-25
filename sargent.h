
enum class RendererBackend {
	DX12 = 0,
	//VULKAN = 1,
	//METAL = 2,
};




struct CommandBuffer {

};

struct RenderPassRenderer {
	
};

void draw_subpass(RenderPassRenderer* pass_renderer, DrawArea draw_area, Draw* draws, u32 draw_count)
{
	Shader* current_shader;
	Mesh* current_mesh;
	Viewport current_viewport;
	Scissor current_scissor;

}
