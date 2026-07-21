#include "gui/GUI.hpp"
#include "gui/helper.hpp"
#include <ament_index_cpp/get_package_share_directory.hpp>

// Add global variable definitions to resolve linker errors
ImGuiContext* imguiContext = nullptr;
int g_distance = 0;
int g_battery = 50;
int g_windSpeed = 3;
int g_angle = 0;
int g_bullet = 29;
int g_cameraYaw = 0;
int g_bbox_x = 0;
int g_bbox_y = 0;
int g_bbox_w = 0;
int g_bbox_h = 0;

GUI::~GUI()
{
    for (const auto& sprite : m_uis_map)
	{
		if(sprite.second.texture_id) glDeleteTextures(1, &sprite.second.texture_id);
	}
    if(!m_uis_map.empty()) m_uis_map.clear();

    if(bg_tex_id) glDeleteTextures(1, &bg_tex_id);

    if(r_Screen != nullptr) delete r_Screen;
}


GUI::GUI(float width, float height, float startX, float startY, rclcpp::Logger logger) : logger_(logger)
{
    this->width = width;
    this->height = height;

    reloadFont = true;
    std::string pkg_path = ament_index_cpp::get_package_share_directory("display");
    LoadUiData(pkg_path + "/assets/textures");

    RCLCPP_INFO(logger_, "GUI: Starting IMGUI Context2 [%f, %f] @ %p.", width, height, imguiContext);
    
    ImGui::SetCurrentContext(imguiContext);
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.DisplaySize.x = width;
    io.DisplaySize.y = height;

    if(r_Screen == nullptr) r_Screen = new Screen2D();
    r_Screen->LoadShaders();
    r_Screen->SetSizeAndPosition(width, height, startX, startY);
    r_Screen->CreateFrameBufferAndTexture();
}

void GUI::BuildFont()
{
    if(reloadFont == false) return;
    
    ImGui::SetCurrentContext(imguiContext);
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    std::string pkg_path = ament_index_cpp::get_package_share_directory("display");

    fontBold48 = io.Fonts->AddFontFromFileTTF((pkg_path + "/assets/fonts/NotoSans-Bold.ttf").c_str(), 48.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
    fontBold32 = io.Fonts->AddFontFromFileTTF((pkg_path + "/assets/fonts/NotoSans-Bold.ttf").c_str(), 32.0f, nullptr, io.Fonts->GetGlyphRangesDefault());

    io.Fonts->Build();

    reloadFont = false;
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
}

void GUI::UpdateBGTextureID(unsigned int id)
{
    bg_tex_id = id;
}

void GUI::LoadUiData(std::string ui_path)
{
	std::string ui_list_path = ui_path + std::string("/ui_list.txt");
	std::ifstream data_file(ui_list_path.c_str());
	std::string name = "";

	if(data_file.is_open())
	{
		while (data_file >> name)
		{
			if (m_uis_map.find(name) == m_uis_map.end() && std::strncmp(&name[0],"#",1))
			{
				std::string file_name = ui_path + std::string("/") + name + std::string(".png");
                m_uis_map[name].texture_id = Helper::LoadTexture(file_name, &m_uis_map[name].width, &m_uis_map[name].height, false);
			}
		}
        data_file.close();
	}
	else
	{
		printf("$Failed to load ui_list.txt!\n");
	}
}

unsigned int GUI::GetUiTextureId(std::string name)
{
    return (m_uis_map.find(name) != m_uis_map.end()) ? m_uis_map[name].texture_id : 0;
}

ImVec2 GUI::GetUiTextureSize(std::string name)
{
    return (m_uis_map.find(name) != m_uis_map.end()) ? ImVec2(m_uis_map[name].width, m_uis_map[name].height) : ImVec2(0.0f, 0.0f);
}


void GUI::Render()
{
    if(!visible) return;

    glBindFramebuffer(GL_FRAMEBUFFER, r_Screen->fb_id);

    ImGui::SetCurrentContext(imguiContext);
    
    BuildFont();
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);

    auto w_size = ImVec2(width, height);
    ImGui::SetNextWindowSize(w_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always, ImVec2(0, 0));

    ImGui::Begin("Main UI", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // ImVec2 bg_size = GetUiTextureSize("03_Main_bg");
    // ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    // ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_Main_bg"), bg_size);

    // printf("bg_tex_id: %d\n", bg_tex_id);
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::Image((ImTextureID)(intptr_t)bg_tex_id, w_size);
   
    
    ImVec2 top_size = GetUiTextureSize("02_Main_top");
    ImGui::SetCursorPos(ImVec2(w_size.x * 0.5f - top_size.x * 0.5f, 0.0f));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("02_Main_top"), top_size);

    ImGui::PushFont(fontBold48);
    std::string distance_text = std::to_string(g_distance) + "m";
    ImVec2 distance_size = ImGui::CalcTextSize(distance_text.c_str());
    ImGui::SetCursorPos(ImVec2(padding, top_size.y * 0.5f - distance_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", distance_text.c_str());
    ImGui::PopFont();

    // compass
    // ImVec2 compass_size = GetUiTextureSize("03_Main_compass");
    ImVec2 compass_size = ImVec2(360.0f, 20.0f);
    float barX = w_size.x * 0.5f - compass_size.x * 0.5f;
    float barY = padding;
    float barWidth = compass_size.x;
    float barHeight = compass_size.y;

    ImGui::SetCursorPos(ImVec2(barX, barY));
    // ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_Main_compass"), compass_size);

    float fullCompassPixelWidth = 360.0f;
    float degreesPerPixel = 120.0f / fullCompassPixelWidth;
    float cameraYaw = fmod(g_cameraYaw, 360.0f);
    if (cameraYaw < 0) cameraYaw += 360.0f;
    float centerOfBarX = w_size.x * 0.5f;


    // Define major directions and their angles
    std::vector<std::pair<std::string, float>> directions = {
        {"N", 0.0f}, {"NE", 45.0f}, {"E", 90.0f}, {"SE", 135.0f},
        {"S", 180.0f}, {"SW", 225.0f}, {"W", 270.0f}, {"NW", 315.0f}
    };

    // To handle wrapping around 360 degrees, duplicate directions for a wider range
    std::vector<std::pair<std::string, float>> extendedDirections;
    for (const auto& dir : directions)
    {
        extendedDirections.push_back({dir.first, dir.second - 360.0f}); // Previous cycle
        extendedDirections.push_back(dir);                              // Current cycle
        extendedDirections.push_back({dir.first, dir.second + 360.0f}); // Next cycle
    }

    // Sort extended directions by angle for easier iteration
    std::sort(extendedDirections.begin(), extendedDirections.end(), [](const auto& a, const auto& b) { return a.second < b.second; });


    // Iterate through a range of degrees visible on the compass bar
    float startDegree = cameraYaw - (barWidth / 2.0f) * degreesPerPixel;
    float endDegree = cameraYaw + (barWidth / 2.0f) * degreesPerPixel;
    float stepDegree = 5.0f;

    for (float deg = floor(startDegree / stepDegree) * stepDegree; deg <= ceil(endDegree / stepDegree) * stepDegree; deg += stepDegree) 
    {
        float currentDeg = fmod(deg, 360.0f);
        if (currentDeg < 0) currentDeg += 360.0f;

        float pixelX = centerOfBarX + (deg - cameraYaw) / degreesPerPixel;

        // Ensure tick is within visible bar width
        if (pixelX >= barX && pixelX <= barX + barWidth)
        {
            float tickWidth = 1.0f;
            float tickHeight = barHeight * 0.5f;
            if (fmod(deg, 45.0f) == 0)
            { 
                tickHeight = barHeight;
            }

            draw_list->AddRect(ImVec2(pixelX, barY), ImVec2(pixelX + tickWidth, barY + tickHeight), IM_COL32(255,255,255,255));
        }
    }


    // Draw labels
    ImGui::PushFont(fontBold32);
    for (const auto& dir : extendedDirections)
    {
        float pixelX = centerOfBarX + (dir.second - cameraYaw) / degreesPerPixel;
        if (pixelX >= barX && pixelX <= barX + barWidth)
        {
            ImGui::SetCursorPos(ImVec2(pixelX - ImGui::CalcTextSize(dir.first.c_str()).x * 0.5f, barY + barHeight + 2.0f));
            ImGui::TextColored(ImVec4(255,255,255,255), "%s", dir.first.c_str());
        }
    }
    ImGui::PopFont();

    ImVec2 battery_size = GetUiTextureSize("03_Main_battery");
    ImGui::SetCursorPos(ImVec2(w_size.x - battery_size.x - padding, top_size.y * 0.5f - battery_size.y * 0.5f));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_Main_battery"), battery_size);


    ImVec2 battery_1step_size = GetUiTextureSize("03_Main_battery_1step");
    for(int i = 0; i < (int)(g_battery * (battery_size.x * 0.75f / battery_1step_size.x) / 100); i++)
    {
        ImGui::SetCursorPos(ImVec2(w_size.x - padding - 7.0f - i * battery_1step_size.x, top_size.y * 0.5f - battery_1step_size.y * 0.5f));
        ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_Main_battery_1step"), battery_1step_size);
    }

    ImGui::PushFont(fontBold48);
    std::string battery_text = std::to_string(g_battery) + "%";
    ImVec2 battery_text_size = ImGui::CalcTextSize(battery_text.c_str());
    ImGui::SetCursorPos(ImVec2(w_size.x - battery_size.x - padding - battery_text_size.x - padding, top_size.y * 0.5f - battery_text_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", battery_text.c_str());
    ImGui::PopFont();


    ImVec2 down_size = GetUiTextureSize("02_Main_down");
    ImGui::SetCursorPos(ImVec2(w_size.x * 0.5f - down_size.x * 0.5f, w_size.y - down_size.y));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("02_Main_down"), down_size);

    ImVec2 compass_icon_size = GetUiTextureSize("03_Main_compass_icon");
    ImGui::SetCursorPos(ImVec2(padding, w_size.y - down_size.y * 0.5f - compass_icon_size.y * 0.5f));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_Main_compass_icon"), compass_icon_size);

    ImGui::PushFont(fontBold48);
    std::string wind_speed_text = std::to_string(g_windSpeed);
    ImVec2 wind_speed_text_size = ImGui::CalcTextSize(wind_speed_text.c_str());
    ImGui::SetCursorPos(ImVec2(padding + compass_icon_size.x + padding, w_size.y - down_size.y * 0.5f - wind_speed_text_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", wind_speed_text.c_str());
    ImGui::PopFont();

    ImGui::PushFont(fontBold32);
    std::string wind_mps_text = "m/s";
    ImVec2 wind_mps_text_size = ImGui::CalcTextSize(wind_mps_text.c_str());
    ImGui::SetCursorPos(ImVec2(padding + compass_icon_size.x + padding + wind_speed_text_size.x + spacing, w_size.y - down_size.y * 0.5f + wind_speed_text_size.y * 0.5f - wind_mps_text_size.y - 4.0f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", wind_mps_text.c_str());
    ImGui::PopFont();

    ImVec2 angle_icon_size = (g_angle >=0) ? GetUiTextureSize("03_Main_up_icon") : GetUiTextureSize("03_Main_down_icon");
    unsigned int angle_icon_tex_id = (g_angle >=0) ? GetUiTextureId("03_Main_up_icon") : GetUiTextureId("03_Main_down_icon");
    ImGui::SetCursorPos(ImVec2(padding * 18.0f, w_size.y - down_size.y * 0.5f - angle_icon_size.y * 0.5f));
    ImGui::Image((ImTextureID)(intptr_t)angle_icon_tex_id, angle_icon_size);

    ImGui::PushFont(fontBold48);
    std::string angle_text = std::to_string(g_angle);
    ImVec2 angle_text_size = ImGui::CalcTextSize(angle_text.c_str());
    ImGui::SetCursorPos(ImVec2(padding * 18.0f + angle_icon_size.x + padding, w_size.y - down_size.y * 0.5f - angle_text_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", angle_text.c_str());
    ImGui::PopFont();

    ImGui::PushFont(fontBold32);
    std::string angle_degree_text = "o";
    ImGui::SetCursorPos(ImVec2(padding * 18.0f + angle_icon_size.x + padding + angle_text_size.x + spacing, w_size.y - down_size.y * 0.5f - angle_text_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", angle_degree_text.c_str());
    ImGui::PopFont();


    ImVec2 bullet_icon_size = GetUiTextureSize("03_main_bullet_icon");
    ImGui::SetCursorPos(ImVec2(w_size.x - padding - bullet_icon_size.x, w_size.y - down_size.y * 0.5f - bullet_icon_size.y * 0.5f));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("03_main_bullet_icon"), bullet_icon_size);

    ImGui::PushFont(fontBold48);
    std::string bullet_text = std::to_string(g_bullet);
    ImVec2 bullet_text_size = ImGui::CalcTextSize(bullet_text.c_str());
    ImGui::SetCursorPos(ImVec2(w_size.x - padding - bullet_icon_size.x - padding - bullet_text_size.x, w_size.y - down_size.y * 0.5f - bullet_text_size.y * 0.5f));
    ImGui::TextColored(ImVec4(255,255,255,255), "%s", bullet_text.c_str());
    ImGui::PopFont();

    ImVec2 target_size = GetUiTextureSize("02_Main_target");
    ImGui::SetCursorPos(ImVec2(w_size.x * 0.5f - target_size.x * 0.5f, w_size.y * 0.5f - target_size.y * 0.5f));
    ImGui::Image((ImTextureID)(intptr_t)GetUiTextureId("02_Main_target"), target_size);


    draw_list->AddRect(ImVec2((float)g_bbox_x, (float)g_bbox_y), ImVec2((float)(g_bbox_x + g_bbox_w), (float)(g_bbox_y + g_bbox_h)), IM_COL32(255,0,0,255), 0.0f, ImDrawFlags_None, 2.0f);


    ImGui::End();
    
    ImGui::PopStyleVar(5);

    // Finish draw and render
    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r_Screen->Render();
}

void GUI::Show()
{
    visible = true;
}

void GUI::Hide()
{
    visible = false;
}
