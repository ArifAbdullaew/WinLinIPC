#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <windows.h>
using namespace std;

void PRINT_ERROR()
{
    LPSTR message;
    DWORD dwMessageLen = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message,
        0,
        NULL);
    cout << '\n'
         << message << ' ';
    HeapFree(GetProcessHeap(), 0, message);
}

void InitializeSecurityAttr(LPSECURITY_ATTRIBUTES attr, SECURITY_DESCRIPTOR *sd)
{
    attr->nLength = sizeof(SECURITY_ATTRIBUTES);
    attr->bInheritHandle = TRUE;
    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    attr->lpSecurityDescriptor = sd;
}

void pipes(vector<int> array, int num_processes)
{
    cout << "PIPES\n";
    int frag_proc = (int)(array.size() / num_processes);
    int frag_last = (int)array.size() - (num_processes - 1) * frag_proc;
    int arr_ind = 0;
    vector<HANDLE> parent_to_child_read;
    vector<HANDLE> parent_to_child_write;
    vector<HANDLE> child_to_parent_read;
    vector<HANDLE> child_to_parent_write;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityAttr(&sa, &sd);
    vector<HANDLE> wait_proc;
    for (int i = 0; i < num_processes; i++)
    {
        HANDLE hParentToChildRead, hParentToChildWrite;
        HANDLE hChildToParentRead, hChildToParentWrite;

        if (!CreatePipe(&hParentToChildRead, &hParentToChildWrite, &sa, 0) ||
            !CreatePipe(&hChildToParentRead, &hChildToParentWrite, &sa, 0))
        {
            cerr << "Error creating pipe\n";
            PRINT_ERROR();
            return;
        }
        parent_to_child_read.push_back(hParentToChildRead);
        parent_to_child_write.push_back(hParentToChildWrite);
        child_to_parent_read.push_back(hChildToParentRead);
        child_to_parent_write.push_back(hChildToParentWrite);

        STARTUPINFOW  si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFOW));
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = parent_to_child_read[i];
        si.hStdOutput = child_to_parent_write[i];

        wchar_t szCmdline[] = L"child.exe pipe";
        BOOL res = CreateProcessW(L"child.exe", szCmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        if (!res)
        {
            cerr << "CreateProcess failed\n";
            PRINT_ERROR();
            return;
        }
        wait_proc.push_back(pi.hProcess);
    }

    int cnt_arr = 0;
    for (int i = 0; i < num_processes; i++)
    {
        stringstream ss;

        if (i != num_processes - 1)
        {
            for (int j = i * frag_proc; j < (i + 1) * frag_proc; j++)
            {
                ss << array[j] << ' ';
            }
        }
        else
        {

            for (int j = i * frag_proc; j < array.size(); j++)
            {
                ss << array[j] << ' ';
            }
        }
        string arrayAsString = ss.str();
        DWORD bytesWritten;
        arrayAsString = arrayAsString + '\0';
        WriteFile(parent_to_child_write[i], arrayAsString.c_str(), arrayAsString.size(), &bytesWritten, NULL);
        CloseHandle(parent_to_child_write[i]);
    }

    int process_sum = 0;
    for (int i = 0; i < num_processes; i++)
    {
        char buffer[1000];
        DWORD bytesRead;
        ReadFile(child_to_parent_read[i], buffer, sizeof(buffer), &bytesRead, NULL);
        CloseHandle(child_to_parent_read[i]);
        CloseHandle(parent_to_child_read[i]);
        CloseHandle(child_to_parent_write[i]);
        if (bytesRead < 0)
        {
            PRINT_ERROR();
            return;
        }
        process_sum += atoi(buffer);
    }
    DWORD dwWaitResult = WaitForMultipleObjects(num_processes, wait_proc.data(), TRUE, INFINITE);
    for (int i = 0; i < num_processes; i++)
    {
        CloseHandle(wait_proc[i]);
    }
    cout << process_sum << '\n';
}

void shared_memory(vector<int> array, int num_processes)
{
    cout << "SM\n";

    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"mutex");

    int frag_proc = (int)(array.size() / num_processes);
    int frag_last = (int)array.size() - (num_processes - 1) * frag_proc;
    int arr_ind = 0;
    // array memory
    HANDLE hMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int) * array.size(), L"array_mem");
    LPVOID lpBaseAddress1 = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int) * array.size());
    int *shared_memory = static_cast<int *>(lpBaseAddress1);
    memcpy(shared_memory, array.data(), array.size() * sizeof(int));

    // sum memory
    HANDLE hMapping1 = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int) * num_processes, L"sum_mem");
    LPVOID lpBaseAddress2 = MapViewOfFile(hMapping1, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int) * num_processes);
    int *shared_memory1 = static_cast<int *>(lpBaseAddress2);

    vector<HANDLE> wait_proc;
    for (int i = 0; i < num_processes; i++)
    {
        int end_len;
        if (i != num_processes - 1)
            end_len = (i + 1) * frag_proc;
        else
            end_len = array.size();
        int t = i * frag_proc;

        STARTUPINFOW  si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFOW ));
        si.cb = sizeof(STARTUPINFOW );
        si.dwFlags = STARTF_USESTDHANDLES;

        // Convert szCmdline to wide char
        wchar_t szCmdline[256];
        swprintf(szCmdline, L"child.exe mem %d %d %d", t, end_len, i);

        BOOL res = CreateProcessW(L"child.exe", szCmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        if (!res)
        {
            cerr << "CreateProcess failed\n";
            PRINT_ERROR();
            return;
        }
        wait_proc.push_back(pi.hProcess);
    }

    DWORD dwWaitResult = WaitForMultipleObjects(num_processes, wait_proc.data(), TRUE, INFINITE);
    for (int i = 0; i < num_processes; i++)
    {
        CloseHandle(wait_proc[i]);
    }
    int proc_sum = 0;
    for (int i = 0; i < num_processes; i++)
    {
        proc_sum += shared_memory1[i];
    }
    UnmapViewOfFile(lpBaseAddress1);
    UnmapViewOfFile(lpBaseAddress2);
    cout << proc_sum << '\n';
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cout << "Usage: <filename> <number_of_processes> <IPC method: 1 - pipe, else - sm>\n";
        return -1;
    }

    int num_processes = stoi(argv[2]);
    ifstream file(argv[1]);
    if (!file)
    {
        cout << "Ошибка открытия файла";
        return -1;
    }
    vector<int> array;

    int number;
    while (file >> number)
    {
        array.push_back(number);
    }
    file.close();

    if (array.size() < 2)
    {
        cout << "File must contain at least 2 numbers";
        return -1;
    }
    if (num_processes > (array.size() / 2))
    {
        num_processes = array.size() / 2;
        cout << "The number of processes is greater than the number of data divided by 2\n";
    }
    int method = stoi(argv[3]);
    if (method == 1)
        pipes(array, num_processes);
    else
        shared_memory(array, num_processes);

    return 0;
}
