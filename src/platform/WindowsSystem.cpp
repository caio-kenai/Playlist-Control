#include "platform/WindowsSystem.h"
#include "platform/WinInclude.h"

#include <tlhelp32.h>

namespace pc
{

namespace
{
juce::String readRegString (HKEY key, const wchar_t* name)
{
    wchar_t buffer[1024] {};
    DWORD size = sizeof (buffer) - sizeof (wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW (key, name, nullptr, &type, reinterpret_cast<LPBYTE> (buffer), &size) != ERROR_SUCCESS)
        return {};
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return {};
    return juce::String (buffer).trim();
}

void collectUninstall (HKEY root, const wchar_t* path, REGSAM view, std::vector<InstalledProgram>& out)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW (root, path, 0, KEY_READ | view, &key) != ERROR_SUCCESS)
        return;
    for (DWORD i = 0;; ++i)
    {
        wchar_t name[256];
        DWORD len = 256;
        if (RegEnumKeyExW (key, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;
        HKEY sub = nullptr;
        if (RegOpenKeyExW (key, name, 0, KEY_READ | view, &sub) != ERROR_SUCCESS)
            continue;
        InstalledProgram p;
        p.displayName = readRegString (sub, L"DisplayName");
        p.version = readRegString (sub, L"DisplayVersion");
        p.publisher = readRegString (sub, L"Publisher");
        p.installLocation = readRegString (sub, L"InstallLocation");
        RegCloseKey (sub);
        if (p.displayName.isNotEmpty())
            out.push_back (p);
    }
    RegCloseKey (key);
}

ServiceState fromWin (DWORD s)
{
    switch (s)
    {
        case SERVICE_STOPPED:          return ServiceState::stopped;
        case SERVICE_START_PENDING:    return ServiceState::starting;
        case SERVICE_RUNNING:          return ServiceState::running;
        case SERVICE_STOP_PENDING:     return ServiceState::stopping;
        case SERVICE_PAUSED:
        case SERVICE_PAUSE_PENDING:
        case SERVICE_CONTINUE_PENDING: return ServiceState::paused;
        default:                       return ServiceState::unknown;
    }
}
} // namespace

std::vector<InstalledProgram> installedPrograms()
{
    std::vector<InstalledProgram> out;
    const wchar_t* path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    collectUninstall (HKEY_LOCAL_MACHINE, path, KEY_WOW64_64KEY, out);
    collectUninstall (HKEY_LOCAL_MACHINE, path, KEY_WOW64_32KEY, out);
    collectUninstall (HKEY_CURRENT_USER, path, 0, out);
    return out;
}

std::vector<RunningProcess> runningProcesses()
{
    std::vector<RunningProcess> out;
    HANDLE snap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return out;
    PROCESSENTRY32W pe {};
    pe.dwSize = sizeof (pe);
    for (BOOL ok = Process32FirstW (snap, &pe); ok; ok = Process32NextW (snap, &pe))
    {
        RunningProcess p;
        p.pid = pe.th32ProcessID;
        p.exeName = juce::String (pe.szExeFile);
        if (HANDLE h = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID))
        {
            wchar_t path[MAX_PATH * 2];
            DWORD len = (DWORD) std::size (path);
            if (QueryFullProcessImageNameW (h, 0, path, &len))
                p.fullPath = juce::String (path, (size_t) len);
            CloseHandle (h);
        }
        out.push_back (p);
    }
    CloseHandle (snap);
    return out;
}

juce::String toDisplayString (ServiceState state)
{
    switch (state)
    {
        case ServiceState::notInstalled: return "Não instalado";
        case ServiceState::stopped:      return "Parado";
        case ServiceState::starting:     return "Iniciando";
        case ServiceState::running:      return "Em execução";
        case ServiceState::stopping:     return "Parando";
        case ServiceState::paused:       return "Pausado";
        case ServiceState::unknown:      return "Desconhecido";
    }
    return {};
}

std::vector<ServiceInfo> findServices (const juce::StringArray& words)
{
    std::vector<ServiceInfo> out;
    SC_HANDLE scm = OpenSCManagerW (nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (scm == nullptr)
        return out;

    DWORD needed = 0, count = 0, resume = 0;
    EnumServicesStatusExW (scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &needed, &count, &resume, nullptr);
    std::vector<BYTE> buffer (needed + 1024);
    resume = 0;
    if (EnumServicesStatusExW (scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buffer.data(),
                               (DWORD) buffer.size(), &needed, &count, &resume, nullptr))
    {
        auto* items = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*> (buffer.data());
        for (DWORD i = 0; i < count; ++i)
        {
            juce::String name (items[i].lpServiceName), display (items[i].lpDisplayName);
            bool match = false;
            for (auto& w : words)
                match = match || name.containsIgnoreCase (w) || display.containsIgnoreCase (w);
            if (match)
                out.push_back ({ name, display, fromWin (items[i].ServiceStatusProcess.dwCurrentState) });
        }
    }
    CloseServiceHandle (scm);
    return out;
}

juce::String fileVersion (const juce::File& file)
{
    auto path = file.getFullPathName();
    DWORD handle = 0;
    auto size = GetFileVersionInfoSizeW (path.toWideCharPointer(), &handle);
    if (size == 0)
        return {};
    std::vector<BYTE> data (size);
    if (! GetFileVersionInfoW (path.toWideCharPointer(), 0, size, data.data()))
        return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (! VerQueryValueW (data.data(), L"\\", reinterpret_cast<void**> (&info), &len) || info == nullptr)
        return {};
    return juce::String (HIWORD (info->dwFileVersionMS)) + "." + juce::String (LOWORD (info->dwFileVersionMS)) + "."
         + juce::String (HIWORD (info->dwFileVersionLS)) + "." + juce::String (LOWORD (info->dwFileVersionLS));
}

} // namespace pc
