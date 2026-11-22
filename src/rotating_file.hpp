#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <cstdio>

class RotatingFile
{
public:
    std::ofstream out;
    std::string base_path;
    long max_size;
    int index;
    long current_size=0;

    RotatingFile(const std::string& path, long size_mb)
    {
        base_path = path;
        max_size = size_mb * 1024 * 1024;
        index = 0;
        current_size= get_file_size();
        out.open(base_path, std::ios::out | std::ios::trunc);
    }

    long get_file_size()
    {
        struct stat st;
        if(stat(base_path.c_str(), &st) == 0)
            return st.st_size;

        return 0;
    }

  void rotate_if_needed(size_t last_write_size)
{
    current_size += last_write_size;

    if(current_size >= max_size)
    {
        out.close();

        std::string new_name =
            base_path + "." + std::to_string(++index);

            std::remove(new_name.c_str());
        std::rename(base_path.c_str(), new_name.c_str());

        out.open(base_path, std::ios::out | std::ios::trunc);
        current_size = 0;
        if(!out.is_open()){
            std::cerr << "Failed to open log file\n";
        }
    }
}
};