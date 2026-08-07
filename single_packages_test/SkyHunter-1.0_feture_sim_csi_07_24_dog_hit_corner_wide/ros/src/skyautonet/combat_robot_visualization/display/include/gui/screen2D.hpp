#ifndef __SCR_2D__
#define __SCR_2D__

#include "helper.hpp"
#include "stb/stb_image.h"

class GLSL
{
    public:

    unsigned int program = 0;
    unsigned int VBO = 0;
    unsigned int VAO = 0;

    int opacity;
    int move_pos;
    int color;
    int screen_size;
    int pos_size;
};

class Screen2D
{
public:
    static GLSL glsl[2];                    // 0 for normal, 1 for vertical flip
    static std::vector<Screen2D*> objs;

    int obj_index;
    int glsl_idx = 0;

    unsigned int fb_id = 0;
    unsigned int tex_id = 0;
    unsigned int bak_tex_id = 0;

    float size[2]{1920.0f, 1080.0f};        //expressed in pixels
    float center[2]{960.0f, 540.0f};        //expressed in pixels
    float additional_move[2]{0.0f, 0.0f};
    float opacity = 1.0f;

    Screen2D(bool verticle_flip = false);

    void LoadShaders();
    void SetSizeAndPosition(float width, float height, float startX = 0.0f, float startY = 0.0f);

    void UpdateTextureID(unsigned int id = 0);

    void CreateFrameBufferAndTexture();

    void Render();
};


#endif
