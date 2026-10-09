#ifndef __HELPER__
#define __HELPER__


#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <cstring>
#include <algorithm>



#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <GLES3/gl31.h>

#ifndef PROJECT_SRC_DIR
    #define PROJECT_SRC_DIR ".."
#endif

namespace Helper
{
    std::string ReadFileAsString(std::string filename);
    unsigned int CompileGLProgram(std::string vertex_shader_file, std::string fragment_shader_file);
    unsigned int LoadTexture(std::string filename, bool vertical_flip = true);
    unsigned int LoadTexture(std::string texture_filename, int* texture_width, int* texture_height, bool vertical_flip = true);
};


#endif //__HELPER__
