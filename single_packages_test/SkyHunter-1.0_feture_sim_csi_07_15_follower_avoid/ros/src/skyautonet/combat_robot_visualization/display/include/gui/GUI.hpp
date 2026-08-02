#ifndef __GUI__
#define __GUI__

#include "backends/imgui_impl_opengl3.h"
#include "screen2D.hpp"
#include <rclcpp/rclcpp.hpp>

class Screen2D;

struct TEXTURE_INFO
{
	int width;
    int height;
	unsigned int texture_id;
};

class GUI
{
public:
    bool visible = false;
    float width = 0.0f;
    float height = 0.0f;
    float padding = 10.0f;
    float spacing = 2.0f;

    unsigned int bg_tex_id = 0;

    ImFont * fontBold48 = nullptr;
    ImFont * fontBold32 = nullptr;

    ~GUI();
    GUI(float width, float height, float startX, float startY, rclcpp::Logger logger);
    void UpdateBGTextureID(unsigned int id);
    void Render();
    void Show();
    void Hide();

private:
    rclcpp::Logger logger_;
    Screen2D * r_Screen = nullptr;
    
    bool reloadFont = false;
    void BuildFont();

    std::map<std::string, TEXTURE_INFO> m_uis_map;
	void LoadUiData(std::string ui_path);
    unsigned int GetUiTextureId(std::string name);
    ImVec2 GetUiTextureSize(std::string name);
};


#endif
