// The setup's own installer for a release folder (native.cpp). It works on a thread and reports in the event lines
// of install.py.
#ifndef PORT_SETUP_NATIVE_H
#define PORT_SETUP_NATIVE_H
#include <string>
#include <vector>
struct NativeTitleLogo { int width=0,height=0; std::vector<unsigned char> pixels; };
NativeTitleLogo Native_ReadTitleLogo(const std::string &path,bool japanese);
// Android-only: refresh touch artwork from already installed archives.
bool Native_PrepareTouchArtwork(const std::string &root);

// root: the folder with zenkaipower and zenkaipower.dat, ends in '/'. iso: the USA disc image (empty: the game is set up already);
// eu, wii and jp: optional disc images; empty omits a disc. An empty USA image adds content to an existing install.
// Release extraction is native for all discs; Wii ISO/RVZ access uses the statically linked nod reader.
// Read-only USA program checksum validation; empty result means success.
std::string Native_VerifyUs(const std::string &path);
bool Native_VerifyInstalledUs(const std::string &root);
// Disc indices: USA, PS2 EU, Wii EU, PS2 JP. Optional hashes match the owner's reference dumps.
std::string Native_VerifyDisc(const std::string &root, const std::string &path, int disc);
void Native_Start(const std::string &root, const std::string &iso, const std::string &eu,
                  const std::string &wii = "", const std::string &jp = "", bool desktopShortcut=false);
bool Native_Poll(std::string &line);                               // the next event line, if there is one
bool Native_Running(void);
void Native_Cancel(void);
void Native_Join(void);                                            // at exit: cancel and wait
bool Native_DiscInstalled(const std::string &root, int disc);
bool Native_Installed(const std::string &root);                    // the game data is unpacked already
bool Native_NeedsShellAssets(const std::string &root);              // missing Windows icons/shortcuts
bool Native_EuInstalled(const std::string &root);                  // the European disc's entries are there

#endif
