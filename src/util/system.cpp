
#include "util/pch.h"

#if defined(PLATFORM_WINDOWS)
    #include <Windows.h>
    #include <commdlg.h>
    #include <iostream>
    #include <tchar.h>              // For _T() macros
#elif defined(PLATFORM_LINUX)
    #include <sys/types.h>          // For pid_t
    #include <sys/wait.h>           // For waitpid
    #include <unistd.h>             // For fork, execv, etc.
    #include <QApplication>
    #include <QFileDialog>
    #include <QString>
    #include <sys/time.h>
    #include <ctime>
    #include <limits.h>
#else
    #error "OS not supported"
#endif

#include "system.h"


namespace GLT::util {

    void open_console(const char* title, const bool enable_anci_codes) {
    
#if defined(PLATFORM_WINDOWS)
        
        AllocConsole();
        FILE* p_file;
        freopen_s(&p_file, "CONOUT$", "w", stdout);
        freopen_s(&p_file, "CONOUT$", "w", stderr);
        freopen_s(&p_file, "CONIN$", "r", stdin);

        std::cout.clear();                                      // Clear the error state for each of the C++ standard stream objects
        std::cerr.clear();
        std::cin.clear();

        SetConsoleTitleA(title);

        if (!enable_anci_codes)
            return;

        // Enable ANSI escape codes for the console
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE)
            std::cerr << "Error: Could not get handle to console output." << std::endl;

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode))
            std::cerr << "Error: Could not get console mode." << std::endl;

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode))
            std::cerr << "Error: Could not set console mode to enable ANSI escape codes." << std::endl;

#elif defined(PLATFORM_LINUX)

        // On Linux, the standard streams are typically already connected to the terminal so we don't need to allocate a new console like in Windows
        std::cout.clear();
        std::cerr.clear();
        std::cin.clear();
        
        if (isatty(STDOUT_FILENO)) {                                // Set terminal title if we're running in a terminal
            std::cout << "\033]0;" << title << "\007";
            std::cout.flush();
        }
        
        // [enable_anci_codes] - ANSI codes are typically enabled by default on Linux terminals so we don't need to do anything special for enable_anci_codes
#endif
    }

         
    std::vector<std::string> parse_arguments(const std::string& cmd) {

        std::vector<std::string> args;
        std::string arg;
        bool in_quotes = false;
        for (char c : cmd) {
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (std::isspace(c) && !in_quotes) {
                if (!arg.empty()) {
                    args.push_back(arg);
                    arg.clear();
                }
            } else {
                arg += c;
            }
        }
        if (!arg.empty()) {
            args.push_back(arg);
        }
        return args;
    }

    //
    bool run_program(const std::filesystem::path& path_to_exe, const std::string& cmd_args, bool open_console, const bool display_output_on_succees, const bool display_output_on_failure, std::string* output) { return run_program(path_to_exe, cmd_args.c_str(), open_console, display_output_on_succees, display_output_on_failure, output); }

    //
    bool run_program(const std::filesystem::path& path_to_exe, const char* cmd_args, bool open_console, const bool display_output_on_succees, const bool display_output_on_failure, std::string* output) {

        //LOG(Trace, "executing program at [" << path_to_exe.generic_string() << "]");

#if defined(PLATFORM_WINDOWS)

        STARTUPINFOA startupInfo;
        PROCESS_INFORMATION processInfo;

        ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);
        ZeroMemory(&processInfo, sizeof(processInfo));

        std::string cmdArguments = path_to_exe.generic_string() + " " + cmd_args;
        auto working_dir = util::get_executable_path().generic_string();

        // Start the program
        bool result = CreateProcessA(
            NULL,							            // Application Name
            (LPSTR)cmdArguments.c_str(),	            // Command Line Args
            NULL,							            // Process Attributes
            NULL,							            // Thread Attributes
            FALSE,							            // Inherit Handles
            (open_console) ? CREATE_NEW_CONSOLE : 0,	// Creation Flags
            NULL,							            // Environment
            working_dir.c_str(),			            // Current Directory
            &startupInfo,					            // Startup Info
            &processInfo					            // Process Info
        );

        WaitForSingleObject(processInfo.hProcess, INFINITE);                                        // Wait for the process to finish

        if (result) {                                                                               // Close process and thread handles

            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
        } else
            LOG(Error, "Unsuccessfully started process: " << path_to_exe.generic_string());

        return true;

#elif defined(PLATFORM_LINUX)
           
        // Build the command string and vector of args
        std::string cmdArguments = path_to_exe.generic_string() + " " + cmd_args;
        // std::vector<std::string> args = parse_arguments(cmdArguments);
        std::istringstream iss(cmdArguments);
        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }

        std::vector<char*> execArgs;
        for (auto& a : args) {
            execArgs.push_back(&a[0]);  // Note: &a[0] is safe while a exists.
        }
        execArgs.push_back(nullptr);

        // Create a pipe to capture stdout and stderr
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            std::cerr << "Failed to create pipe." << std::endl;
            return false;
        }

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork process." << std::endl;
            return false;

        } else if (pid == 0) {                                                          // Child process

            close(pipefd[0]);                                                           // Close the read end of the pipe; not needed in the child.
            dup2(pipefd[1], STDOUT_FILENO);                                             // Redirect stdout and stderr to the pipe's write end.
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);                                                           // Close after duplicating

            if (open_console)
                execlp("xterm", "xterm", "-e", execArgs[0], (char*)nullptr);
            else
                execv(path_to_exe.c_str(), execArgs.data());

            std::cerr << "Failed to execute program: " << path_to_exe.generic_string() << std::endl;
            exit(EXIT_FAILURE);

        } else {                                                                        // Parent process

            close(pipefd[1]);                                                           // Close the write end of the pipe.

            // Read output from the pipe
            constexpr size_t bufferSize = 1024;
            char buffer[bufferSize];
            ssize_t count;
            while ((count = read(pipefd[0], buffer, bufferSize - 1)) > 0) {
                buffer[count] = '\0';
                output->append(buffer);
            }
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);

            return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
        }

#endif

    }


    void high_precision_sleep(f32 duration_in_milliseconds) {

        static const f32 estimated_deviation = 10.0f;
        auto loc_duration_in_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<f32>(duration_in_milliseconds)).count();
        auto target_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(static_cast<int>(loc_duration_in_milliseconds));

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(loc_duration_in_milliseconds - estimated_deviation)));

        // Busy wait for the remaining time
        while (std::chrono::high_resolution_clock::now() < target_time)
            ;

        //auto actual_sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - target_time + std::chrono::milliseconds(static_cast<int>(duration_in_milliseconds)) ).count();
        //LOG(Debug, "left over time: " << actual_sleep_time << " ms");
    }


    system_time get_system_time() {

        system_time loc_system_time{};

#if defined(PLATFORM_WINDOWS)

        SYSTEMTIME win_time;
        GetLocalTime(&win_time);
        loc_system_time.year = static_cast<u16>(win_time.wYear);
        loc_system_time.month = static_cast<u8>(win_time.wMonth);
        loc_system_time.day = static_cast<u8>(win_time.wDay);
        loc_system_time.day_of_week = static_cast<u8>(win_time.wDayOfWeek);
        loc_system_time.hour = static_cast<u8>(win_time.wHour);
        loc_system_time.minute = static_cast<u8>(win_time.wMinute);
        loc_system_time.secund = static_cast<u8>(win_time.wSecond);
        loc_system_time.millisecend = static_cast<u16>(win_time.wMilliseconds);

#elif defined(PLATFORM_LINUX)

        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm* ptm = localtime(&tv.tv_sec);
        loc_system_time.year = static_cast<u16>(ptm->tm_year + 1900);
        loc_system_time.month = static_cast<u8>(ptm->tm_mon + 1);
        loc_system_time.day = static_cast<u8>(ptm->tm_mday);
        loc_system_time.day_of_week = static_cast<u8>(ptm->tm_wday);
        loc_system_time.hour = static_cast<u8>(ptm->tm_hour);
        loc_system_time.minute = static_cast<u8>(ptm->tm_min);
        loc_system_time.secund = static_cast<u8>(ptm->tm_sec);
        loc_system_time.millisecend = static_cast<u16>(tv.tv_usec / 1000);

#endif
        return loc_system_time;
    }


#if defined(PLATFORM_LINUX)                                     // QT related functions (Linux only)

    namespace {
        std::unique_ptr<QApplication> qt_app;
        int qt_argc = 1;
        char qt_argv0[] = "editor";  // Permanent storage for argv[0]
        char* qt_argv[] = {qt_argv0, nullptr};
    }

    void qt_message_handler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
        
        // Map Qt message type to your severity levels
        logger::severity sev = logger::severity::Debug;
        switch(type) {
            case QtDebugMsg:    sev = logger::severity::Debug; break;
            case QtInfoMsg:     sev = logger::severity::Info; break;
            case QtWarningMsg:  sev = logger::severity::Warn; break;
            case QtCriticalMsg: sev = logger::severity::Error; break;
            case QtFatalMsg:    sev = logger::severity::Fatal; break;
        }
        
        // Extract context information
        const char* file            = context.file ? context.file : "none";
        const char* function        = context.function ? context.function : "none";
        int line                    = context.line;
        std::thread::id threadId    = std::this_thread::get_id();
        std::string message         = msg.toStdString();
        logger::log_msg(sev, file, function, line, threadId, std::move(message));
    }
    
    void init_qt() {
            
        LOG(Trace, "Initiating QT");
    
        qInstallMessageHandler(qt_message_handler);
        
        if (!qt_app) {
            QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
            qt_app = std::make_unique<QApplication>(qt_argc, qt_argv);
        }
    }
    
    void shutdown_qt() { 

        if (qt_app) {
            qInstallMessageHandler(nullptr);
            qt_app->quit();
            qt_app.reset();
        }
    }

#endif


    std::filesystem::path file_dialog(const std::string& title, const std::vector<std::pair<std::string, std::string>>& filters) {

    #if defined(PLATFORM_WINDOWS)
        // Assuming you have a way to get the handle of your main window
        HWND hwndOwner = GetActiveWindow(); // or your main window handle

        OPENFILENAME ofn;                                                       // common dialog box structure
        wchar_t szFile[260] = { 0 };                                            // Using wchar_t instead of char for Unicode support

        ZeroMemory(&ofn, sizeof(ofn));                                          // initialize OPENFILENAME
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwndOwner;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);

        std::wstring filter;
        for (const auto& f : filters)                                           // create filter string
            filter += std::wstring(f.first.begin(), f.first.end()) + L'\0' + std::wstring(f.second.begin(), f.second.end()) + L'\0';
        filter += L'\0';

        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        std::wstring wtitle(title.begin(), title.end());                        // set dialog title
        ofn.lpstrTitle = wtitle.c_str();

        if (GetOpenFileNameW(&ofn) == TRUE)
            return std::filesystem::path(szFile);

        return std::filesystem::path();

    #elif defined(PLATFORM_LINUX)
   
         if (!qt_app) {
            LOG(Error, "QApplication not initialized!");
            return {};
        }

        QString filterString;
        for (auto& filter : filters) {                                                                                                  // Prepare the filter string for QFileDialog
            
            // Replace semicolons with spaces
            std::string buffer = filter.second;
            size_t pos = 0;
            while ((pos = buffer.find(';', pos)) != std::string::npos) {                                                                // Replace ';' with ' ' 
                buffer.replace(pos, 1, " ");
                pos += 1;
            }
            
            filterString += QString::fromStdString(filter.first) + " (" + QString::fromStdString(buffer) + ");;";
        }
        filterString.chop(2); // Remove the last ";;"

        QString fileName = QFileDialog::getOpenFileName(nullptr, QString::fromUtf8(title.data()), QString(), filterString);             // Open the file dialog
        return std::filesystem::path(fileName.toStdString());

    #endif
    }


    std::vector<std::filesystem::path> file_dialog_multi(const std::string& title, const std::vector<std::pair<std::string, std::string>>& filters) {

#if defined(PLATFORM_WINDOWS)

        HWND hwndOwner = GetActiveWindow();
        OPENFILENAMEW ofn;
        std::vector<std::wstring> filterW;
        for (auto& f : filters)
            filterW.push_back(std::wstring(f.first.begin(), f.first.end()) + L'\0' +
                              std::wstring(f.second.begin(), f.second.end()) + L'\0');
        // Concatenate filters and double‑null terminate
        std::wstring filterStr;
        for (auto& s : filterW) filterStr += s;
        filterStr += L'\0';
    
        wchar_t szFiles[4096] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = hwndOwner;
        ofn.lpstrFile    = szFiles;
        ofn.nMaxFile     = sizeof(szFiles) / sizeof(wchar_t);
        ofn.lpstrFilter  = filterStr.c_str();
        ofn.nFilterIndex = 1;
        ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                          | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
        std::wstring wtitle(title.begin(), title.end());
        ofn.lpstrTitle   = wtitle.c_str();
    
        if (GetOpenFileNameW(&ofn)) {
            std::vector<std::filesystem::path> results;
            std::wstring dir = szFiles;
            wchar_t* p = szFiles + dir.size() + 1;
            if (*p == L'\0') {
                // Only one file selected
                results.emplace_back(dir);
            } else {
                // Multiple files: parse names after directory
                while (*p) {
                    results.emplace_back(std::filesystem::path(dir) / p);
                    p += wcslen(p) + 1;
                }
            }
            return results;
        }
        return {};
    
#elif defined(PLATFORM_LINUX)

         if (!qt_app) {
            LOG(Error, "QApplication not initialized!");
            return {};
        }

        QStringList nameFilters;
        for (auto& f : filters) {
            // Replace semicolons in the pattern with spaces
            std::string pat = f.second;
            std::replace(pat.begin(), pat.end(), ';', ' ');
            nameFilters << QString::fromStdString(f.first + " (" + pat + ")");
        }
        QStringList files = QFileDialog::getOpenFileNames(
            nullptr,
            QString::fromUtf8(title.data()),
            QString(),
            nameFilters.join(";;")
        );
        std::vector<std::filesystem::path> results;
        for (const auto& qf : files)
            results.emplace_back(qf.toStdString());
        return results;

#endif
    }
    

    std::filesystem::path get_executable_path() {

    #if defined(PLATFORM_WINDOWS)

        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(NULL, path, MAX_PATH)) {
            std::filesystem::path execPath(path);
            return execPath.parent_path();
        }

    #elif defined(PLATFORM_LINUX)
        
        char path[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        if (count != -1) {
            path[count] = '\0'; // Null-terminate the string
            std::filesystem::path execPath(path);
            return execPath.parent_path();
        }

    #endif

        std::cerr << "Error retrieving the executable path." << std::endl;
        return std::filesystem::path();
    }


}
