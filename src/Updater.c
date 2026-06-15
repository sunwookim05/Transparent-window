#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Updater.h"

#define UPDATE_API_HOST L"api.github.com"
#define UPDATE_API_PATH L"/repos/sunwookim05/Transparent-window/releases/latest"
#define UPDATE_USER_AGENT L"SystemTransparency/1.0.2"

typedef struct {
    string data;
    DWORD size;
} HttpBuffer;

static void updateLog(string message) {
    char tempPath[MAX_PATH];
    char logPath[MAX_PATH];
    SYSTEMTIME time;
    FILE* file;

    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return;

    if ((size_t)snprintf(logPath, sizeof(logPath), "%s%sUpdate.log", tempPath, APP_NAME) >= sizeof(logPath))
        return;

    GetLocalTime(&time);
    file = fopen(logPath, "a");
    if (!file)
        return;

    fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, message);
    fclose(file);
}

static string duplicateRange(string start, string end) {
    size_t len;
    string out;

    if (!start || !end || end < start)
        return null;

    len = (size_t)(end - start);
    out = (string)malloc(len + 1);
    if (!out)
        return null;

    memcpy(out, start, len);
    out[len] = '\0';

    return out;
}

static wchar_t* utf8ToWide(string text) {
    int len;
    wchar_t* out;

    len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (len <= 0)
        return null;

    out = (wchar_t*)malloc((size_t)len * sizeof(wchar_t));
    if (!out)
        return null;

    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, len);
    return out;
}

static boolean appendHttpData(HttpBuffer* buffer, const void* data, DWORD size) {
    string next;

    next = (string)realloc(buffer->data, buffer->size + size + 1);
    if (!next)
        return false;

    buffer->data = next;
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';

    return true;
}

static boolean readResponse(HINTERNET request, HttpBuffer* buffer) {
    DWORD available;
    DWORD read;
    char chunk[8192];

    buffer->data = null;
    buffer->size = 0;

    while (true) {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
            return false;

        if (available == 0)
            break;

        while (available > 0) {
            DWORD toRead = min(available, sizeof(chunk));

            if (!WinHttpReadData(request, chunk, toRead, &read))
                return false;

            if (read == 0)
                break;

            if (!appendHttpData(buffer, chunk, read))
                return false;

            available -= read;
        }
    }

    return buffer->data != null;
}

static boolean httpGet(const wchar_t* host, const wchar_t* path, HttpBuffer* buffer) {
    HINTERNET session = null;
    HINTERNET connect = null;
    HINTERNET request = null;
    BOOL ok;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);

    session = WinHttpOpen(UPDATE_USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        goto fail;

    connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect)
        goto fail;

    request = WinHttpOpenRequest(connect, L"GET", path, null,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request)
        goto fail;

    ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok)
        goto fail;

    if (!WinHttpReceiveResponse(request, null))
        goto fail;

    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        null, &status, &statusSize, null);
    if (status < 200 || status >= 300)
        goto fail;

    ok = readResponse(request, buffer);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    return ok ? true : false;

fail:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return false;
}

static boolean wideToUtf8(const wchar_t* text, string out, DWORD outSize) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, out, outSize, null, null);
    return len > 0 && (DWORD)len <= outSize;
}

static boolean httpGetUrl(string url, HttpBuffer* buffer) {
    char currentUrl[16384];

    lstrcpynA(currentUrl, url, sizeof(currentUrl));

    for (int redirect = 0; redirect < 8; redirect++) {
        URL_COMPONENTSW parts;
        wchar_t host[512];
        wchar_t path[8192];
        wchar_t extra[8192];
        wchar_t requestPath[16384];
        wchar_t location[16384];
        wchar_t* wideUrl;
        HINTERNET session = null;
        HINTERNET connect = null;
        HINTERNET request = null;
        BOOL ok;
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        DWORD locationSize = sizeof(location);

        wideUrl = utf8ToWide(currentUrl);
        if (!wideUrl)
            return false;

        ZeroMemory(&parts, sizeof(parts));
        ZeroMemory(host, sizeof(host));
        ZeroMemory(path, sizeof(path));
        ZeroMemory(extra, sizeof(extra));

        parts.dwStructSize = sizeof(parts);
        parts.lpszHostName = host;
        parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
        parts.lpszExtraInfo = extra;
        parts.dwExtraInfoLength = sizeof(extra) / sizeof(extra[0]);

        if (!WinHttpCrackUrl(wideUrl, 0, 0, &parts)) {
            free(wideUrl);
            return false;
        }

        if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
            free(wideUrl);
            return false;
        }

        swprintf(requestPath, sizeof(requestPath) / sizeof(requestPath[0]), L"%s%s", path, extra);
        free(wideUrl);

        session = WinHttpOpen(UPDATE_USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;

        connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            return false;
        }

        request = WinHttpOpenRequest(connect, L"GET", requestPath, null,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!ok || !WinHttpReceiveResponse(request, null)) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            null, &status, &statusSize, null);

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, null, location, &locationSize, null)) {
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return false;
            }

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);

            if (!wideToUtf8(location, currentUrl, sizeof(currentUrl)))
                return false;

            continue;
        }

        if (status < 200 || status >= 300) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        ok = readResponse(request, buffer);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        return ok ? true : false;
    }

    return false;
}

static boolean httpDownloadUrl(string url, string outPath) {
    HttpBuffer buffer;
    HANDLE file;
    DWORD written;
    boolean ok = false;

    buffer.data = null;
    buffer.size = 0;

    if (!httpGetUrl(url, &buffer))
        return false;

    file = CreateFileA(outPath, GENERIC_WRITE, 0, null, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, null);
    if (file != INVALID_HANDLE_VALUE) {
        ok = WriteFile(file, buffer.data, buffer.size, &written, null) &&
            written == buffer.size && buffer.size > 0;
        CloseHandle(file);
    }

    free(buffer.data);

    if (!ok)
        DeleteFileA(outPath);

    return ok;
}

static string jsonStringValue(string json, string key) {
    char pattern[128];
    string pos;
    string start;
    string end;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(json, pattern);
    if (!pos)
        return null;

    pos = strchr(pos + strlen(pattern), ':');
    if (!pos)
        return null;

    start = strchr(pos, '"');
    if (!start)
        return null;

    start++;
    end = start;

    while (*end) {
        if (*end == '"' && *(end - 1) != '\\')
            break;
        end++;
    }

    if (*end != '"')
        return null;

    return duplicateRange(start, end);
}

static string findExeAssetUrl(string json) {
    string pos = json;

    while ((pos = strstr(pos, "\"browser_download_url\"")) != null) {
        string url = jsonStringValue(pos, "browser_download_url");

        if (url && strstr(url, APP_EXE_NAME))
            return url;

        free(url);
        pos += 22;
    }

    return null;
}

static void parseVersion(string version, int out[4]) {
    string p = version;

    out[0] = out[1] = out[2] = out[3] = 0;

    while (*p && (*p < '0' || *p > '9'))
        p++;

    sscanf(p, "%d.%d.%d.%d", &out[0], &out[1], &out[2], &out[3]);
}

static boolean isRemoteNewer(string remoteTag) {
    int remote[4];
    int current[4];

    parseVersion(remoteTag, remote);
    parseVersion(APP_VERSION, current);

    for (int i = 0; i < 4; i++) {
        if (remote[i] > current[i]) return true;
        if (remote[i] < current[i]) return false;
    }

    return false;
}

static boolean createUpdateBatch(string currentExe, string newExe) {
    char tempPath[MAX_PATH];
    char batchPath[MAX_PATH];
    FILE* file;
    DWORD pid = GetCurrentProcessId();
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdLine[MAX_PATH + 16];

    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    if ((size_t)snprintf(batchPath, sizeof(batchPath), "%s%sUpdate.bat", tempPath, APP_NAME) >= sizeof(batchPath))
        return false;

    file = fopen(batchPath, "w");
    if (!file)
        return false;

    fprintf(file, "@echo off\n");
    fprintf(file, "set \"PID=%lu\"\n", (unsigned long)pid);
    fprintf(file, "set \"OLD=%s\"\n", currentExe);
    fprintf(file, "set \"NEW=%s\"\n", newExe);
    fprintf(file, ":wait\n");
    fprintf(file, "tasklist /FI \"PID eq %%PID%%\" | findstr \"%%PID%%\" >nul\n");
    fprintf(file, "if not errorlevel 1 (\n");
    fprintf(file, "  timeout /t 1 /nobreak >nul\n");
    fprintf(file, "  goto wait\n");
    fprintf(file, ")\n");
    fprintf(file, "move /y \"%%NEW%%\" \"%%OLD%%\" >nul\n");
    fprintf(file, "start \"\" \"%%OLD%%\"\n");
    fprintf(file, "del \"%%~f0\"\n");
    fclose(file);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    snprintf(cmdLine, sizeof(cmdLine), "cmd.exe /c \"%s\"", batchPath);

    if (!CreateProcessA(null, cmdLine, null, null, false, CREATE_NO_WINDOW,
            null, null, &si, &pi))
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
}

static void cleanupUpdateData(string* tag, string* url, string* jsonData) {
    free(*tag);
    free(*url);
    free(*jsonData);

    *tag = null;
    *url = null;
    *jsonData = null;
}

static void checkNow(Updater* self) {
    HttpBuffer json;
    string tag = null;
    string url = null;
    char installedExe[MAX_PATH];
    char newExe[MAX_PATH];
    char message[512];

    json.data = null;
    json.size = 0;

    updateLog("check started");

    if (!self->installer.isInstalledPath(&self->installer)) {
        updateLog("skipped: not running from installed path");
        return;
    }

    if (!httpGet(UPDATE_API_HOST, UPDATE_API_PATH, &json)) {
        updateLog("failed: latest api request");
        return;
    }

    tag = jsonStringValue(json.data, "tag_name");
    if (!tag || !isRemoteNewer(tag)) {
        if (tag) {
            snprintf(message, sizeof(message), "skipped: remote %s is not newer than %s", tag, APP_VERSION);
            updateLog(message);
        } else {
            updateLog("failed: tag_name missing");
        }
        cleanupUpdateData(&tag, &url, &json.data);
        return;
    }

    snprintf(message, sizeof(message), "new version found: %s", tag);
    updateLog(message);

    url = findExeAssetUrl(json.data);
    if (!url) {
        updateLog("failed: SystemTransparency.exe asset missing");
        cleanupUpdateData(&tag, &url, &json.data);
        return;
    }

    if (!self->installer.getInstalledExePath(&self->installer, installedExe, sizeof(installedExe))) {
        updateLog("failed: installed exe path");
        cleanupUpdateData(&tag, &url, &json.data);
        return;
    }

    if ((size_t)snprintf(newExe, sizeof(newExe), "%s.new.exe", installedExe) >= sizeof(newExe)) {
        updateLog("failed: new exe path overflow");
        cleanupUpdateData(&tag, &url, &json.data);
        return;
    }

    if (!httpDownloadUrl(url, newExe)) {
        updateLog("failed: download asset");
        cleanupUpdateData(&tag, &url, &json.data);
        return;
    }

    cleanupUpdateData(&tag, &url, &json.data);

    updateLog("downloaded asset");

    if (createUpdateBatch(installedExe, newExe)) {
        updateLog("update batch created");
        ExitProcess(0);
    }

    updateLog("failed: create update batch");
}

static DWORD WINAPI updateThreadProc(LPVOID arg) {
    Updater* updater = (Updater*)arg;

    Sleep(5000);
    updater->checkNow(updater);
    free(updater);

    return 0;
}

static void checkAsync(Updater* self) {
    Updater* updater = (Updater*)malloc(sizeof(Updater));
    HANDLE thread;

    if (!updater)
        return;

    *updater = *self;
    thread = CreateThread(null, 0, updateThreadProc, updater, 0, null);

    if (thread)
        CloseHandle(thread);
    else
        free(updater);
}

Updater new_Updater(Installer installer) {
    return (Updater) {
        .installer = installer,
        .checkAsync = checkAsync,
        .checkNow = checkNow
    };
}
