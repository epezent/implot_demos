#pragma once
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <glad/glad.h>

struct Image
{

    Image() : Width(0),
              Height(0),
              ID(0)
    {
    }

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    ~Image()
    {
        if (ID != 0)
            glDeleteTextures(1, &ID);
    }

    bool LoadFromFile(const char *filepath)
    {
        if (ID != 0)
            glDeleteTextures(1, &ID);
        ID = 0;
        unsigned char *image_data = stbi_load(filepath, &Width, &Height, NULL, 4);
        if (image_data == nullptr)
            return false;
        glGenTextures(1, &ID);
        glBindTexture(GL_TEXTURE_2D, ID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);
        return true;
    }

    int Width;
    int Height;
    GLuint ID;
};