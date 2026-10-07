#pragma once
#include <string>
namespace outrun::mac {
struct Paths { std::string saves; };
// Creates the macOS save folder (Application Support). Caches live in the game folder.
bool user_paths(Paths& paths,std::string& error);
// The folder that holds the application: the folder containing OutRun.app, or the
// executable's folder when it is not in a bundle. It is the default game folder.
std::string application_folder();
// Fallback when the application is not in a game folder.
bool choose_retail_directory(std::string& path);
}
