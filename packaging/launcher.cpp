#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
namespace fs=std::filesystem;
std::wstring quote(const std::wstring& s){std::wstring out=L"\"";unsigned slashes=0;for(auto c:s){if(c==L'\\'){++slashes;continue;}out.append(c==L'"'?slashes*2+1:slashes,L'\\');out+=c;slashes=0;}out.append(slashes*2,L'\\');return out+L"\"";}
std::wstring literal(const fs::path& p){std::wstring s=L"'";for(auto c:p.wstring()){s+=c;if(c==L'\'')s+=c;}return s+L"'";}
DWORD run(const fs::path& exe,std::wstring args,const fs::path& cwd){auto command=quote(exe.wstring())+L" "+args;STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi))throw std::runtime_error("CreateProcess failed");CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);return code;}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    HANDLE mutex=nullptr;bool quiet=false;
    try{
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)throw std::runtime_error("Invalid arguments");
        wchar_t local[32768]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768))throw std::runtime_error("LOCALAPPDATA unavailable");
        const bool extract=argc==3&&std::wstring(argv[1])==L"--extract-only";quiet=extract;
        fs::path root=extract?fs::absolute(argv[2]):fs::path(local)/L"Tinglan"/L"Runtime"/PAYLOAD_VERSION;
        mutex=CreateMutexW(nullptr,FALSE,L"Local\\TinglanExtract_" PAYLOAD_VERSION);if(!mutex||WaitForSingleObject(mutex,120000)==WAIT_TIMEOUT)throw std::runtime_error("Runtime is busy");
        if(!fs::exists(root/L"ready.marker")){
            if(fs::exists(root)&&!fs::is_empty(root))throw std::runtime_error("Incomplete runtime folder: choose a new extraction folder");
            auto pending=root;pending+=L".extract-"+std::to_wstring(GetCurrentProcessId());fs::create_directories(pending);
            const auto resource=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);auto loaded=LoadResource(instance,resource);auto bytes=LockResource(loaded);const auto size=SizeofResource(instance,resource);if(!bytes||!size)throw std::runtime_error("Missing runtime payload");
            const auto zip=pending/L"runtime.zip";{std::ofstream f(zip,std::ios::binary);f.write(static_cast<const char*>(bytes),size);if(!f)throw std::runtime_error("Cannot write runtime payload");}
            wchar_t win[32768]{};GetWindowsDirectoryW(win,32768);const fs::path ps=fs::path(win)/L"System32/WindowsPowerShell/v1.0/powershell.exe";
            const auto script=L"$ErrorActionPreference='Stop'; Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory("+literal(zip)+L", "+literal(pending)+L")";
            const auto code=run(ps,L"-NoProfile -NonInteractive -WindowStyle Hidden -Command "+quote(script),pending);if(code!=0)throw std::runtime_error("Cannot extract runtime, PowerShell exit "+std::to_string(code));
            fs::remove(zip);if(!fs::exists(pending/L"im_desktop.exe"))throw std::runtime_error("Incomplete payload");
            {std::ofstream marker(pending/L"ready.marker");marker<<"5.0";}
            if(fs::exists(root))fs::remove(root);fs::rename(pending,root);
        }
        ReleaseMutex(mutex);CloseHandle(mutex);mutex=nullptr;
        std::wstring args;for(int i=1;i<argc;++i){if(i>1)args+=L" ";args+=quote(argv[i]);}LocalFree(argv);
        if(extract)return 0;
        return static_cast<int>(run(root/L"im_desktop.exe",args,root));
    }catch(const std::exception& e){if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}const std::string detail=std::string(e.what())+" (Windows error "+std::to_string(GetLastError())+")";DWORD written=0;WriteFile(GetStdHandle(STD_ERROR_HANDLE),detail.data(),DWORD(detail.size()),&written,nullptr);if(!quiet){wchar_t message[2048]{};MultiByteToWideChar(CP_UTF8,0,detail.c_str(),-1,message,2048);MessageBoxW(nullptr,(std::wstring(L"听澜启动失败，请尝试安装包。\n\n")+message).c_str(),L"听澜",MB_OK|MB_ICONERROR);}return 1;}
}



