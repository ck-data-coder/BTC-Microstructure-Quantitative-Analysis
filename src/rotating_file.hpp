#pragma once

#include <fstream>
#include <string>
#include <sys/stat.h>

class RotatingFile
{
public:
    std::ofstream out;
    std::string base_path;
    long max_size;
    int index;

    RotatingFile(const std::string& path, long size_mb)
    {
        base_path = path;
        max_size = size_mb * 1024 * 1024;
        index = 0;

        out.open(base_path, std::ios::out | std::ios::app);
    }

    long get_file_size()
    {
        struct stat st;
        if(stat(base_path.c_str(), &st) == 0)
            return st.st_size;

        return 0;
    }

    void rotate_if_needed()
    {
        if(get_file_size() >= max_size)
        {
            out.close();

            std::string new_name =
                base_path + "." + std::to_string(index++);

            rename(base_path.c_str(), new_name.c_str());

            out.open(base_path, std::ios::out | std::ios::app);
        }
    }
};