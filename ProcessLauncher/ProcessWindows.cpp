#include "ProcessWindows.h"
#include <windows.h>

namespace ugly
{
    namespace process
    {

        Pipe::Pipe()
            : Pipe(Inherit::None)
        { }
        
        Pipe::Pipe(Inherit inheritance)
        {
            SECURITY_ATTRIBUTES pipeAttributes;
            pipeAttributes.nLength = sizeof(pipeAttributes);
            pipeAttributes.bInheritHandle = TRUE;
            pipeAttributes.lpSecurityDescriptor = NULL;
            valid = !!CreatePipe(&read, &write, &pipeAttributes, 16384);
            if (valid)
            {
                if (!(inheritance & Inherit::Read))
                    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
                if (!(inheritance & Inherit::Write))
                    SetHandleInformation(write, HANDLE_FLAG_INHERIT, 0);
            }
        }

        Pipe::~Pipe()
        {
            if (valid)
            {
                CloseHandle(read);
                CloseHandle(write);
            }
        }

        ProcessWindows::ProcessWindows()
            : processStdInPipe(Pipe::Inherit::Read)
            , processStdOutPipe(Pipe::Inherit::Write)
        {
        }

        bool ProcessWindows::TryCreate()
        {
            if (!processStdInPipe.isValid() || !processStdOutPipe.isValid())
                return false;
            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.hStdInput = processStdInPipe.getReadHandle();
            si.hStdOutput = processStdOutPipe.getWriteHandle();
            si.hStdError = processStdOutPipe.getWriteHandle();
            si.dwFlags |= STARTF_USESTDHANDLES;
            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(pi));
            if (!CreateProcessA(GetExecutable().c_str(), const_cast<char*>(GetArguments().c_str()), NULL, NULL, FALSE, DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi))
                return false;
            process = pi.hProcess;
            processId = pi.dwProcessId;
            return true;
        }

        bool ProcessWindows::TryStart()
        {
            return !!DebugActiveProcessStop(processId);
        }

        bool ProcessWindows::TryStop()
        {
            return !!DebugActiveProcess(processId);
        }

        bool ProcessWindows::TryKill()
        {
            return !!TerminateProcess(process, 1);
        }

        std::string ProcessWindows::ReadLine(std::chrono::high_resolution_clock::duration timeout)
        {
            char buffer[16384];
            DWORD read = 0;
            OVERLAPPED overlapped;
            ZeroMemory(&overlapped, sizeof(overlapped));
            ReadFile(processStdOutPipe.getReadHandle(), &buffer, sizeof(buffer), &read, &overlapped);
            if (!GetOverlappedResultEx(processStdOutPipe.getReadHandle(), &overlapped, &read, (DWORD)(timeout.count() * 1000 * std::chrono::high_resolution_clock::period::num / std::chrono::high_resolution_clock::period::den), FALSE))
            {
                CancelIoEx(processStdOutPipe.getReadHandle(), &overlapped);
                return "";
            }
            return std::string(buffer, read);
        }

        bool ProcessWindows::TryWrite(const std::string& data)
        {
            DWORD wrote = 0;
            return !!WriteFile(processStdInPipe.getWriteHandle(), data.c_str(), (DWORD)data.size(), &wrote, NULL);
        }
    }
}