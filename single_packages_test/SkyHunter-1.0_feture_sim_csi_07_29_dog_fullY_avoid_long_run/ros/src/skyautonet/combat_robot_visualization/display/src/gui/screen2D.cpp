#include "gui/screen2D.hpp"
#include "gui/helper.hpp"
#include <ament_index_cpp/get_package_share_directory.hpp>

GLSL Screen2D::glsl[2];
std::vector<Screen2D*> Screen2D::objs;

Screen2D::Screen2D(bool verticle_flip)
{
    obj_index = (int)objs.size();
    objs.push_back(this);

    glsl_idx = (int)verticle_flip;

    // Generate buffer data
    if(glsl[glsl_idx].VAO == 0)
    {
        GLfloat vertices[16];
        if(verticle_flip == true)
        {
            GLfloat vtx[16] = {
                -1.0f, -1.0f, 0.0f, 1.0f,
                +1.0f, -1.0f, 1.0f, 1.0f,
                -1.0f, +1.0f, 0.0f, 0.0f,
                +1.0f, +1.0f, 1.0f, 0.0f,
            };
            std::copy(std::begin(vtx), std::end(vtx), std::begin(vertices));
        }
        else
        {
            GLfloat vtx[16] = {
                -1.0f, +1.0f, 0.0f, 1.0f,
                +1.0f, +1.0f, 1.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f,
                +1.0f, -1.0f, 1.0f, 0.0f,
            };
            std::copy(std::begin(vtx), std::end(vtx), std::begin(vertices));
        }

        glGenVertexArrays(1, &glsl[glsl_idx].VAO);
        glGenBuffers(1, &glsl[glsl_idx].VBO);

        glBindVertexArray(glsl[glsl_idx].VAO);
        glBindBuffer(GL_ARRAY_BUFFER, glsl[glsl_idx].VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
}

void Screen2D::LoadShaders()
{
    if(glsl[glsl_idx].program == 0)
    {
        std::string pkg_path = ament_index_cpp::get_package_share_directory("display");
        glsl[glsl_idx].program = Helper::CompileGLProgram(pkg_path + "/assets/shaders/screen2d.vert", pkg_path + "/assets/shaders/screen2d.frag");
        glUseProgram(glsl[glsl_idx].program);
        glUniform1i(glGetUniformLocation(glsl[glsl_idx].program, "tex"), 0);
        glsl[glsl_idx].opacity = glGetUniformLocation(glsl[glsl_idx].program, "opacity");
        glsl[glsl_idx].move_pos = glGetUniformLocation(glsl[glsl_idx].program, "additional_move");
        glsl[glsl_idx].screen_size = glGetUniformLocation(glsl[glsl_idx].program, "screen_size");
        glsl[glsl_idx].pos_size = glGetUniformLocation(glsl[glsl_idx].program, "pos_size");
    }
}

void Screen2D::SetSizeAndPosition(float width, float height, float startX, float startY)
{
    size[0] = width;
    size[1] = height;
    center[0] = size[0] * 0.5f;
    center[1] = size[1] * 0.5f;
    additional_move[0] = startX;
    additional_move[1] = startY;
}

void Screen2D::UpdateTextureID(unsigned int id)
{
    if(id == 0) // reverse back to fb texture id
    {
        tex_id = bak_tex_id;
    }
    else
    {
        bak_tex_id = tex_id;
        tex_id = id;
    }
}

void Screen2D::CreateFrameBufferAndTexture()
{
    glGenFramebuffers(1, &fb_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_id);

    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    // Allocate storage for the texture (adjust width and height as needed)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Attach the texture to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

    // Check if the framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer is not complete!");

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Screen2D::Render()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(glsl[glsl_idx].program);

    glUniform1f(glsl[glsl_idx].opacity, opacity);
    glUniform2fv(glsl[glsl_idx].move_pos, 1, additional_move);
    glUniform2f(glsl[glsl_idx].screen_size, (float)size[0], (float)size[1]);
    glUniform4f(glsl[glsl_idx].pos_size, center[0], center[1], size[0], size[1]);

    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glBindVertexArray(glsl[glsl_idx].VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(GL_BLEND);
}
