#ifndef _GUI_H_
#define _GUI_H_

#include <string>
#include <vector>

// 前方宣言（インクルード依存を最小化）
#ifdef SUPERMODEL_WIN32
class RemoteSlotManager;
#endif

#ifdef SUPERMODEL_WIN32
std::vector<std::string> RunGUI(const std::string& configPath, Util::Config::Node& config, RemoteSlotManager* pRemote);
#else
std::vector<std::string> RunGUI(const std::string& configPath, Util::Config::Node& config);
#endif

#endif // !_GUI_H_
