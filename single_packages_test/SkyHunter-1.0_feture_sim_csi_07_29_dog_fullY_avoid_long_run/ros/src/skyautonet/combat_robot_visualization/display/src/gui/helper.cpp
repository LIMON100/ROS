#include "gui/helper.hpp"

#define STB_IMAGE_IMPLEMENTATION
    #include "stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

namespace Helper
{
    std::string ReadFileAsString(std::string filename)
    {
        FILE *file = fopen(filename.c_str(), "r");
        if (file == NULL) {
            perror("Failed to open the file");
            return "";
        }

        // Seek to the end of the file
        fseek(file, 0, SEEK_END);
        // Determine file size
        long filesize = ftell(file);
        // Seek back to the start of the file
        fseek(file, 0, SEEK_SET);

        // Allocate memory to store the entire file
        char * content =  (char*)malloc(filesize + 1);
        if (content == NULL) {
            perror("Failed to allocate memory");
            fclose(file);
            return "";
        }

        // Read the file into the allocated memory
        auto size = fread(content, 1, filesize, file);
        content[filesize] = '\0';  // Null-terminate the read content

        std::string val = std::string(content);

        // Cleanup
        free((void*)content);
        fclose(file);
        (void) size;
        
        return val;
    }

    unsigned int CompileGLProgram(std::string vertex_shader_file, std::string fragment_shader_file)
    {
        unsigned int program = 0;
        std::string shader_vert = ReadFileAsString(vertex_shader_file);
        std::string shader_frag = ReadFileAsString(fragment_shader_file);
    
        const char* vertex_shader_text = shader_vert.c_str();
        const char* fragment_shader_text = shader_frag.c_str();

        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
        glCompileShader(vertex_shader);
    
        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
        glCompileShader(fragment_shader);


        GLint isCompiled = 0;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &isCompiled);
        if(isCompiled == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &maxLength);

            // The maxLength includes the NULL character
            auto errorLog = new char[maxLength];
            glGetShaderInfoLog(fragment_shader, maxLength, &maxLength, errorLog);

            // Provide the infolog in whatever manor you deem best.
            // Exit with failure.
            glDeleteShader(fragment_shader); // Don't leak the shader.

            printf("%s\n", fragment_shader_file.c_str());
            printf("Unable to compile OpenGL fragment shader: \n%s\n\n.", errorLog);
            return 0;
        }


        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &isCompiled);
        if(isCompiled == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &maxLength);

            // The maxLength includes the NULL character
            auto errorLog = new char[maxLength];
            glGetShaderInfoLog(vertex_shader, maxLength, &maxLength, errorLog);

            // Provide the infolog in whatever manor you deem best.
            // Exit with failure.
            glDeleteShader(vertex_shader); // Don't leak the shader.
            printf("Unable to compile OpenGL vertex shader: \n%s\n\n", errorLog);
            delete [] errorLog;
            return 0;
        }

        
        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        GLint linkSuccess;
        glGetProgramiv(program, GL_LINK_STATUS, &linkSuccess);

        if (!linkSuccess)
        {
            GLint logLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

            char *logMessage = new char[logLength];
            glGetProgramInfoLog(program, logLength, NULL, logMessage);

            printf("Program Linking Error: %s.\n", logMessage);

            delete[] logMessage;
        }
        //printf("Programs %s and %s compiled to [%d].\n", vertex_shader_file.c_str(), fragment_shader_file.c_str(), program);
        return program;
    }

    unsigned int LoadTexture(std::string texture_filename, bool vertical_flip)
    {
        unsigned int texture = 0;
        if((int)texture_filename.length() > 0)
        {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            // set the texture wrapping/filtering options (on the currently bound texture object)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            // load and generate the texture
            int width, height, nrChannels;
            
            stbi_set_flip_vertically_on_load(vertical_flip);
            
            unsigned char *data = stbi_load(texture_filename.c_str(), &width, &height, &nrChannels, 0);
            if (data)
            {
                auto format = (nrChannels==4?GL_RGBA:GL_RGB);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            else
            {
                printf("Failed to load texture %s.\n", texture_filename.c_str());
            }
            stbi_set_flip_vertically_on_load(false);
            stbi_image_free(data);
        }
        return texture;
    }


    unsigned int LoadTexture(std::string texture_filename, int* texture_width, int* texture_height, bool vertical_flip)
    {
        unsigned int texture = 0;
        int width = 0, height = 0, nrChannels = 0;

        if((int)texture_filename.length() > 0)
        {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            // set the texture wrapping/filtering options (on the currently bound texture object)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            // load and generate the texture
            
            stbi_set_flip_vertically_on_load(vertical_flip);
            
            unsigned char *data = stbi_load(texture_filename.c_str(), &width, &height, &nrChannels, 0);
            if (data)
            {
                auto format = (nrChannels==4?GL_RGBA:GL_RGB);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            else
            {
                printf("Failed to load texture %s.\n", texture_filename.c_str());
            }
            stbi_set_flip_vertically_on_load(false);
            stbi_image_free(data);
        }

        *texture_width = width;
        *texture_height = height;

        return texture;
    }




};
