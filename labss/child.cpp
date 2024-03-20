#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <fcntl.h> 
#include <unistd.h>
#include <windows.h>
#include <winbase.h>
using namespace std;

SIZE_T check_size(string s){
    HANDLE hMapping = OpenFileMappingA (FILE_MAP_READ , FALSE, "array_mem");
    LPVOID lpBaseAddress = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (lpBaseAddress == NULL) {
        printf("Failed to map view of file (%d)\n", GetLastError());
        CloseHandle(hMapping);
        return 1;
    }
    MEMORY_BASIC_INFORMATION memInfo;
    if (VirtualQuery(lpBaseAddress, &memInfo, sizeof(memInfo)) == 0) {
        printf("VirtualQuery failed (%d)\n", GetLastError());
        UnmapViewOfFile(lpBaseAddress);
        CloseHandle(hMapping);
        return 1;
    }
    SIZE_T regionSize = memInfo.RegionSize;
    UnmapViewOfFile(lpBaseAddress);
    CloseHandle(hMapping);
}

int main(int argc, char *argv[]) {
    string t = argv[1];
    if (t == "mem"){
        SIZE_T s1 = check_size("array_mem");
        HANDLE hMapping = OpenFileMappingA (FILE_MAP_READ , FALSE, "array_mem");
        LPVOID lpBaseAddress = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, s1);
        int * shared_memory = static_cast<int*>(lpBaseAddress);
        
        // //mutex
        HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, L"mutex");
        //sum
        int sum = 0;
        int start = atoi(argv[2]), end = atoi(argv[3]);

        WaitForSingleObject(mutex, INFINITE);
        for (size_t i = start; i < end; i++) {
            sum += shared_memory[i];
        }
        ReleaseMutex(mutex);
        UnmapViewOfFile(lpBaseAddress);
        CloseHandle(hMapping);

        // //write mem
        SIZE_T s2 = check_size("sum_mem");
        HANDLE hMapping1 = OpenFileMappingA (FILE_MAP_WRITE , FALSE, "sum_mem");
        LPVOID lpBaseAddress1 = MapViewOfFile(hMapping1, FILE_MAP_WRITE, 0, 0, s2);
        int * shared_memory1 = static_cast<int*>(lpBaseAddress1);
        int pos = atoi(argv[4]);

        WaitForSingleObject(mutex, INFINITE);
        shared_memory1[pos] = sum; 
        ReleaseMutex(mutex);

        UnmapViewOfFile(lpBaseAddress1);
        CloseHandle(hMapping1);   
        CloseHandle(mutex);
    }
    else{
        HANDLE child_write_pipe, child_read_pipe;
        char message_from_parent[100];
        DWORD bytes_read;

        child_write_pipe = GetStdHandle(STD_INPUT_HANDLE);
        ReadFile(child_write_pipe, message_from_parent, sizeof(message_from_parent), &bytes_read, NULL);

        int sum = 0;
        int number;

        istringstream iss(message_from_parent);
        while (iss >> number) {
            sum += number;
            while (iss.peek() == ' ') {
                iss.ignore();
            }
        }
        string sum_str = to_string(sum) + '\0';
        cout << sum_str << endl;
    }

    exit(EXIT_SUCCESS);
}