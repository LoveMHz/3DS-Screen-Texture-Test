#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include "vshader_shbin.h"
#include "kitten_bin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct { float position[3]; float texcoord[2]; float normal[3]; } vertex;

static const vertex vertex_list[] =
{

	{ {400.0f, 240.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {  0.0f, 240.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {  0.0f,   0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },

	{ {  0.0f,   0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, +1.0f} },
	{ {400.0f,   0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
	{ {400.0f, 240.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, +1.0f} },
};

#define vertex_list_count (sizeof(vertex_list)/sizeof(vertex_list[0]))

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;

static void* vbo_data;
static C3D_Tex kitten_tex;

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);


	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v2=normal

	// Compute the projection matrix
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0);

	// Create the VBO (vertex buffer object)
	vbo_data = linearAlloc(sizeof(vertex_list));
	memcpy(vbo_data, vertex_list, sizeof(vertex_list));

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, vbo_data, sizeof(vertex), 3, 0x210);

	int tw = 64;
	int th = 64;

	// Load the texture and bind it to the first texture unit
	C3D_TexInit(&kitten_tex, tw, th, GPU_RGBA8);

	for(int x = 0; x < tw; x++)
	{
		for(int y = 0; y < th; y++)
		{
			*(int*)(kitten_tex.data + (tw * y * 4) + (x * 4) + 0) = 0xFF0000FF;
		}
	}

	//C3D_TexUpload(&kitten_tex, kitten_bin);
	C3D_TexSetFilter(&kitten_tex, GPU_LINEAR, GPU_NEAREST);
	C3D_TexBind(0, &kitten_tex);

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

static void sceneRender(void)
{
	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	// Draw the VBO
	C3D_DrawArrays(GPU_TRIANGLES, 0, vertex_list_count);
}

static void sceneExit(void)
{
	// Free the texture
	C3D_TexDelete(&kitten_tex);

	// Free the VBO
	linearFree(vbo_data);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

int main()
{
	// Initialize graphics
	gfxInitDefault();

	//Initialize console on bottom screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_BOTTOM, NULL);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	// Initialize the renderbuffer
	static C3D_RenderBuf rb;
	C3D_RenderBufInit(&rb, 240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	rb.clearColor = CLEAR_COLOR;
	C3D_RenderBufClear(&rb);
	C3D_RenderBufBind(&rb);

	// Initialize the scene
	sceneInit();

	// Main loop
	while (aptMainLoop())
	{
		C3D_VideoSync();
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		// Render the scene
		sceneRender();
		C3D_Flush();
		C3D_RenderBufTransfer(&rb, (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), DISPLAY_TRANSFER_FLAGS);
		C3D_RenderBufClear(&rb);
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
