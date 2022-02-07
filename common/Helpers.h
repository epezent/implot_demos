#pragma once

#include <math.h>
#include <cstdlib>
#include <imgui.h>
#include <imgui_internal.h>

template <typename T>
inline T RandomRange(T min, T max) {
    T scale = rand() / (T) RAND_MAX;
    return min + scale * ( max - min );
}

inline ImVec4 RandomColor(float alpha = 1.0f) {
    ImVec4 col;
    col.x = RandomRange(0.0f,1.0f);
    col.y = RandomRange(0.0f,1.0f);
    col.z = RandomRange(0.0f,1.0f);
    col.w = alpha;
    return col;
}

inline bool GetBranchName(const std::string &git_repo_path, std::string &branch_name)
{
    std::string head_path = git_repo_path + "/.git/HEAD";
    size_t head_size = 0;
    bool result = false;
    if (char *git_head = (char *)ImFileLoadToMemory(head_path.c_str(), "r", &head_size, 1))
    {
        const char prefix[] = "ref: refs/heads/";
        const int prefix_length = IM_ARRAYSIZE(prefix) - 1;
        strtok(git_head, "\r\n");
        if (head_size > prefix_length && strncmp(git_head, prefix, prefix_length) == 0)
        {
            branch_name = (git_head + prefix_length);
            result = true;
        }
        IM_FREE(git_head);
    }
    return result;
}