#pragma once
#include <string>
struct SetupUpdate {
    std::string version, url, digest, error;
    bool available = false;
};
SetupUpdate SetupUpdate_Check(const std::string &installed, const std::string &source);
// Canonical GitHub repository URL from game-info's source; empty for invalid/non-GitHub URLs.
std::string SetupUpdate_Repository(const std::string &source);
// Downloads and validates an update, then launches a helper which waits for setup to exit.
std::string SetupUpdate_Prepare(const SetupUpdate &update, const std::string &root);
// Returns -1 for a normal launch; otherwise handles the update helper before the UI starts.
int SetupUpdate_Helper(int argc, char **argv);
bool SetupUpdate_Newer(const std::string &candidate, const std::string &installed);
