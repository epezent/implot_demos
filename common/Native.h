#pragma once
#include <nfd.hpp>
#include <string>
#include <vector>

/// Result of a file dialog function
enum DialogResult { DialogError, DialogOkay, DialogCancel };

/// Filtetype filter, a friendly name string follow by a series of comma separated extensions
/// e.g. DialogFilter filter({"Audio Files","wav,mp3"});
typedef nfdu8filteritem_t DialogFilter;

/// Opens a native file save dialog
DialogResult SaveDialog(std::string& out_path, 
                        const std::vector<DialogFilter>& filters = {},
                        const std::string& default_path = "",
                        const std::string& default_name = "");

/// Opens a native single file open dialog
DialogResult OpenDialog(std::string& out_path, 
                        const std::vector<DialogFilter>& filters = {},
                        const std::string& default_path = "");

/// Opens a native multiple file open dialog
DialogResult OpenDialog(std::vector<std::string>&        out_paths,
                        const std::vector<DialogFilter>& filters      = {},
                        const std::string&               default_path = "");

/// Opens a native folder selection dialog
DialogResult PickDialog(std::string& out_path, const std::string& default_path = "");

enum SysDir {
    UserProfile,
    AppDataRoaming,
    AppDataLocal,
    AppDataTemp,
    ProgramData,
    ProgramFiles,
    ProgramFilesX86,
};

/// Returns the common application data directory
const std::string& GetSysDir(SysDir dir);

/// Opens a folder in the native file explorer
bool OpenFolder(const std::string& path);

/// Opens a file with the default application
bool OpenFile(const std::string& path);

/// Opens an external link to a URL
void OpenUrl(const std::string& url);

/// Opens an external link to an email
void OpenEmail(const std::string& address, const std::string& subject = "");