#include "display.hpp"


// Place this at the top of the file, outside any namespace
extern ImGuiContext* imguiContext;
extern int g_distance;
extern int g_battery;
extern int g_windSpeed;
extern int g_angle;
extern int g_bullet;
extern int g_cameraYaw;
extern int g_bbox_x;
extern int g_bbox_y;
extern int g_bbox_w;
extern int g_bbox_h;

namespace combat_robot_visualization {

DisplayNode::DisplayNode(const rclcpp::NodeOptions& options)
: Node("display_node", options),
  display_(nullptr),
  egl_display_(EGL_NO_DISPLAY),
  egl_context_(EGL_NO_CONTEXT),
  egl_surface_(EGL_NO_SURFACE),
  gui_(nullptr),
  main_screen_(nullptr),
  imgui_context_(nullptr),
  camera_texture_(0)
{
    rclcpp::QoS qos_profile(rclcpp::KeepLast(10));
    qos_profile.reliability(rclcpp::ReliabilityPolicy::BestEffort);
    qos_profile.durability(rclcpp::DurabilityPolicy::Volatile);

    // Initialize camera image subscriber
    camera_sub_ = create_subscription<sensor_msgs::msg::Image>(
        "/human_detector/human/image_raw", qos_profile,
        std::bind(&DisplayNode::imageCallback, this, std::placeholders::_1));
    // Initialize subscribers
    distance_sub_ = create_subscription<std_msgs::msg::Float64>(
        "sensor/distance", 10,
        std::bind(&DisplayNode::distanceCallback, this, std::placeholders::_1));
    
    battery_sub_ = create_subscription<std_msgs::msg::Int32>(
        "sensor/battery", 10,
        std::bind(&DisplayNode::batteryCallback, this, std::placeholders::_1));
        
    wind_speed_sub_ = create_subscription<std_msgs::msg::Int32>(
        "sensor/wind_speed", 10,
        std::bind(&DisplayNode::windSpeedCallback, this, std::placeholders::_1));
        
    bullet_sub_ = create_subscription<std_msgs::msg::Int32>(
        "sensor/bullet", 10,
        std::bind(&DisplayNode::bulletCallback, this, std::placeholders::_1));
    
    // Initialize display and GUI
    if (!initX11andEGL()) {
        RCLCPP_ERROR(get_logger(), "Failed to initialize X11 and EGL");
        return;
    }

    // --- START: Add startup screen logic from original main.cpp ---
    eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);

    // Create a temporary Screen2D for the startup image
    Screen2D startup_screen(true);
    startup_screen.LoadShaders();
    startup_screen.SetSizeAndPosition(800.0f, 480.0f, 0.0f, 0.0f);

    // Load the startup texture
    std::string pkg_path = ament_index_cpp::get_package_share_directory("display");
    unsigned int startup_tex_id = Helper::LoadTexture(pkg_path + "/assets/textures/01_Intro_Sample.png", false);

    // Render the startup screen
    if (startup_tex_id != 0) {
        startup_screen.UpdateTextureID(startup_tex_id);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Clear to black
        glClear(GL_COLOR_BUFFER_BIT);

        startup_screen.Render();
        eglSwapBuffers(egl_display_, egl_surface_);

        // Display for a short duration
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Clean up the texture
        glDeleteTextures(1, &startup_tex_id);
    } else {
        RCLCPP_WARN(get_logger(), "Could not load startup texture: %s", (pkg_path + "/assets/textures/01_Intro_Sample.png").c_str());
    }
    // --- END: Add startup screen logic ---

    initGUI();
    // Create render timer (30 FPS)
    render_timer_ = create_wall_timer(
        std::chrono::milliseconds(33),
        std::bind(&DisplayNode::renderCallback, this));
    RCLCPP_INFO(get_logger(), "Display node initialized successfully");
}

DisplayNode::~DisplayNode() {
    // Make the context current to ensure cleanup functions work correctly
    if (egl_display_ != EGL_NO_DISPLAY && egl_surface_ != EGL_NO_SURFACE && egl_context_ != EGL_NO_CONTEXT) {
        eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
    }

    // Shutdown ImGui in reverse order of initialization
    if (imgui_context_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext(imgui_context_);
        imgui_context_ = nullptr;
    }

    // Clean up GUI and screen resources
    if (gui_) delete gui_;
    if (main_screen_) delete main_screen_;
    if (egl_surface_ != EGL_NO_SURFACE)
        eglDestroySurface(egl_display_, egl_surface_);
    if (egl_context_ != EGL_NO_CONTEXT)
        eglDestroyContext(egl_display_, egl_context_);
    if (x11_window_)
        XDestroyWindow(display_, x11_window_);
    if (egl_display_ != EGL_NO_DISPLAY)
        eglTerminate(egl_display_);
    if (display_)
        XCloseDisplay(display_);
}

bool DisplayNode::initX11andEGL() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        RCLCPP_ERROR(get_logger(), "Failed to open X display");
        return false;
    }

    // Initialize EGL
    egl_display_ = eglGetDisplay(display_);
    if (egl_display_ == EGL_NO_DISPLAY) {
        RCLCPP_ERROR(get_logger(), "Failed to get EGL display");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(egl_display_, &major, &minor)) {
        RCLCPP_ERROR(get_logger(), "Failed to initialize EGL");
        return false;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    // Configure EGL
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    EGLConfig config;
    if (!eglChooseConfig(egl_display_, configAttribs, &config, 1, &numConfigs)) {
        RCLCPP_ERROR(get_logger(), "Failed to choose EGL config");
        return false;
    }

    // Create X11 window
    Window root = DefaultRootWindow(display_);
    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    // Listen for more events for a more robust window
    attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | PointerMotionMask;
    // x11_window_ = XCreateWindow(
    //     display_, root, 0, 0, 800, 480, 0, // Initial position and size
    //     CopyFromParent, InputOutput,
    //     CopyFromParent, CWBackPixel | CWEventMask,
    //     &attr);
    Screen* screen = DefaultScreenOfDisplay(display_);
    int screen_w = screen->width;
    int screen_h = screen->height;
    x11_window_ = XCreateWindow(
        display_, root, 0, 0, screen_w, screen_h, 0,
        CopyFromParent, InputOutput,
        CopyFromParent, CWBackPixel | CWEventMask,
        &attr);
    XStoreName(display_, x11_window_, "Combat Robot Visualization");

    // --- Start: Code to make window borderless and fullscreen ---
    // This is similar to your provided createNewX11EGLWindow function

    // Hide title bar using _MOTIF_WM_HINTS
    struct MwmHints {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    };
    enum { MWM_HINTS_DECORATIONS = (1L << 1) };
    Atom mwmHintsProperty = XInternAtom(display_, "_MOTIF_WM_HINTS", False);
    struct MwmHints hints = {0};
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = 0; // Set to 0 to remove decorations
    XChangeProperty(display_, x11_window_, mwmHintsProperty, mwmHintsProperty, 32,
                   PropModeReplace, (unsigned char *)&hints, 5);

    // Set fullscreen using _NET_WM_STATE
    Atom wm_state = XInternAtom(display_, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(display_, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = x11_window_;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0;
    XSendEvent(display_, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

    // --- End: Code for borderless and fullscreen ---

    if (!x11_window_) {
        RCLCPP_ERROR(get_logger(), "Failed to create X11 window");
        return false;
    }

    XMapWindow(display_, x11_window_);

    // Create EGL surface
    egl_surface_ = eglCreateWindowSurface(egl_display_, config, x11_window_, NULL);
    if (egl_surface_ == EGL_NO_SURFACE) {
        RCLCPP_ERROR(get_logger(), "Failed to create EGL surface");
        return false;
    }

    // Create EGL context
    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    egl_context_ = eglCreateContext(egl_display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
        RCLCPP_ERROR(get_logger(), "Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
        RCLCPP_ERROR(get_logger(), "Failed to make EGL context current");
        return false;
    }

    return true;
}

void DisplayNode::initGUI() {
    eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    imgui_context_ = ImGui::CreateContext();
    
    // The global 'imguiContext' (defined in GUI.cpp) must be set BEFORE the GUI object is created.
    // This ensures the GUI constructor can access the valid context.
    imguiContext = imgui_context_;

    ImGui::SetCurrentContext(imgui_context_);
    ImGui::StyleColorsDark();
    EGLint surf_w = 0, surf_h = 0;
    eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH,  &surf_w);
    eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &surf_h);

    // Setup Platform/Renderer backends
    ImGui_ImplOpenGL3_Init("#version 100"); // Renderer backend for OpenGL ES

    // Create GUI instance
    if (gui_ == nullptr)
    {
        RCLCPP_INFO(this->get_logger(), "Create GUI() instance");
        //gui_ = new GUI(800.0f, 480.0f, 0.0f, 0.0f, this->get_logger());
        gui_ = new GUI(static_cast<float>(surf_w), static_cast<float>(surf_h), 0.0f, 0.0f, this->get_logger());
        gui_->Show();
    }

    if (!gui_) {
        RCLCPP_ERROR(get_logger(), "Failed to create GUI instance");
        return;
    }
}

void DisplayNode::renderCallback() {
    if (!gui_ || !egl_display_ || !egl_surface_) {
        return;
    }

    std::lock_guard<std::mutex> lock(gl_mutex_);

    // 1) 컨텍스트 먼저 바인딩
    eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);

    // 2) 표적 크기 동기화
    EGLint w = 0, h = 0;
    eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH,  &w);
    eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &h);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    glViewport(0, 0, w, h);

    // 3) (중요) 카메라 텍스처는 컨텍스트 바인딩 후에 업로드
    if (gui_) {
        if (camera_texture_ != 0) {
            glDeleteTextures(1, &camera_texture_);
        }
        glGenTextures(1, &camera_texture_);
        glBindTexture(GL_TEXTURE_2D, camera_texture_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // 행 패딩 이슈 방지 (RGB 3바이트 정렬)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // OpenCV Mat가 연속 메모리인지 확인 권장 (필요하면 clone()해서 연속화)
        // if (!camera_frame_.isContinuous()) camera_frame_ = camera_frame_.clone();

        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGB,
            camera_frame_.cols, camera_frame_.rows,
            0, GL_RGB, GL_UNSIGNED_BYTE, camera_frame_.data
        );

        gui_->UpdateBGTextureID(camera_texture_);
    }

    // 4) ImGui 프레임 시작
    ImGui_ImplOpenGL3_NewFrame();
    //ImGui_ImplEGL_NewFrame();
    ImGui::NewFrame();

    gui_->Render();

    ImGui::Render();

    // 5) 그리기 전에 전체 화면 클리어 (Scissor 영향 배제)
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 6) ImGui 드로우 + 스왑
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(egl_display_, egl_surface_);
}


void DisplayNode::imageCallback(const sensor_msgs::msg::Image::SharedPtr msg)
{
    try
    {
        // Lock the mutex to ensure thread-safe access to OpenGL resources
        std::lock_guard<std::mutex> lock(gl_mutex_); 
        cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::RGB8);
        camera_frame_ = cv_ptr->image.clone();    
    }
    catch (const cv_bridge::Exception& e)
    {
        RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
    }
}

// Callback implementations
// Update global variables that the GUI reads from.
void DisplayNode::distanceCallback(const std_msgs::msg::Float64::SharedPtr msg) {
    g_distance = static_cast<int32_t>(msg->data);
}

void DisplayNode::batteryCallback(const std_msgs::msg::Int32::SharedPtr msg) {
    g_battery = msg->data;
}

void DisplayNode::windSpeedCallback(const std_msgs::msg::Int32::SharedPtr msg) {
    g_windSpeed = msg->data;
}

void DisplayNode::bulletCallback(const std_msgs::msg::Int32::SharedPtr msg) {
    g_bullet = msg->data;
} 
}  // namespace combat_robot_visualization
