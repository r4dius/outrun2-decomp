#pragma once
// File system helpers the port needs beyond <cstdio>.
#include <string>
#include <vector>
namespace outrun::platform {
// Creates one directory; true when it exists afterwards.
bool make_directory(const std::string& path);
bool is_directory(const std::string& path);
// Entry names of a directory ("." and ".." included); *opened tells whether it
// could be read.
std::vector<std::string> directory_names(const std::string& dir,bool* opened=nullptr);
// Removes an empty directory; false when it is missing or not empty.
bool remove_directory(const std::string& path);
}
