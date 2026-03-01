// SharedMemoryServer_Fixed.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>

#define BUFFER_SIZE 256
#define SHM_NAME L"Global\\ChatSharedMemory12345"
#define MUTEX_NAME L"Global\\ChatMutex12345"
#define READY_EVENT_NAME L"Global\\ChatReadyEvent12345"
#define MESSAGE_EVENT_NAME L"Global\\ChatMessageEvent12345"

struct SharedData {
    char message[BUFFER_SIZE];
    bool serverRunning;
    bool clientConnected;
};

// Helper function to convert wide string to narrow string
std::string WideToNarrow(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

int main() {
    // Option 1: Use wcout for all wide string output
    std::wcout << L"=== SHARED MEMORY CHAT SERVER ===" << std::endl;
    std::wcout << L"Starting server..." << std::endl;

    // 1. Create shared memory
    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,   // Use paging file
        NULL,                   // Default security
        PAGE_READWRITE,         // Read/write access
        0,                      // Maximum object size (high-order DWORD)
        sizeof(SharedData),     // Maximum object size (low-order DWORD)
        SHM_NAME                // Name of mapping object
    );

    if (hMapFile == NULL) {
        DWORD error = GetLastError();
        std::wcerr << L"Could not create file mapping object. Error: " << error << std::endl;
        return 1;
    }

    // 2. Map the shared memory
    SharedData* pData = (SharedData*)MapViewOfFile(
        hMapFile,               // Handle to map object
        FILE_MAP_ALL_ACCESS,    // Read/write permission
        0,
        0,
        sizeof(SharedData)
    );

    if (pData == NULL) {
        DWORD error = GetLastError();
        std::wcerr << L"Could not map view of file. Error: " << error << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // 3. Initialize shared data
    ZeroMemory(pData, sizeof(SharedData));
    pData->serverRunning = true;
    pData->clientConnected = false;
    strcpy_s(pData->message, "Server started. Waiting for client...");

    // 4. Create mutex for synchronization
    HANDLE hMutex = CreateMutex(
        NULL,                   // Default security attributes
        FALSE,                  // Initially not owned
        MUTEX_NAME              // Named mutex
    );

    if (hMutex == NULL) {
        DWORD error = GetLastError();
        std::wcerr << L"CreateMutex error: " << error << std::endl;
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        return 1;
    }

    // 5. Create events for signaling
    HANDLE hReadyEvent = CreateEvent(
        NULL,                   // Default security attributes
        TRUE,                   // Manual-reset event
        FALSE,                  // Initial state is nonsignaled
        READY_EVENT_NAME        // Event name
    );

    HANDLE hMessageEvent = CreateEvent(
        NULL,                   // Default security attributes
        TRUE,                   // Manual-reset event
        FALSE,                  // Initial state is nonsignaled
        MESSAGE_EVENT_NAME      // Event name
    );

    // FIXED: Proper output of wide string
    std::wcout << L"Server created successfully!" << std::endl;
    std::wcout << L"Shared memory name: " << SHM_NAME << std::endl;
    std::wcout << L"Mutex name: " << MUTEX_NAME << std::endl;
    std::wcout << L"Waiting for client to connect..." << std::endl;

    // Signal that server is ready
    SetEvent(hReadyEvent);

    // Wait for client to connect
    bool clientConnected = false;
    while (!clientConnected) {
        WaitForSingleObject(hMutex, INFINITE);
        if (pData->clientConnected) {
            clientConnected = true;
            std::cout << "Client connected!" << std::endl;
        }
        ReleaseMutex(hMutex);
        Sleep(100);
    }

    // Start chat
    std::cout << "\n=== CHAT STARTED ===" << std::endl;
    std::cout << "Type your messages (type 'exit' to quit):" << std::endl;

    std::thread receiver([&]() {
        while (pData->serverRunning) {
            // Wait for message from client
            DWORD waitResult = WaitForSingleObject(hMessageEvent, 100);

            if (waitResult == WAIT_OBJECT_0) {
                WaitForSingleObject(hMutex, INFINITE);

                if (strlen(pData->message) > 0 && !pData->clientConnected) {
                    // Message from client
                    std::cout << "\n[Client]: " << pData->message << std::endl;
                    std::cout << "[Server]: ";

                    // Clear the message and reset event
                    pData->message[0] = '\0';
                    ResetEvent(hMessageEvent);
                }

                ReleaseMutex(hMutex);
            }
            Sleep(50);
        }
        });

    // Main sending loop
    std::string input;
    while (pData->serverRunning) {
        std::cout << "[Server]: ";
        std::getline(std::cin, input);

        if (input == "exit") {
            pData->serverRunning = false;
            break;
        }

        WaitForSingleObject(hMutex, INFINITE);
        strcpy_s(pData->message, input.c_str());
        pData->clientConnected = false;  // Flag that server sent message
        ReleaseMutex(hMutex);

        // Signal client
        SetEvent(hMessageEvent);
    }

    // Cleanup
    if (receiver.joinable()) {
        receiver.join();
    }

    std::cout << "Shutting down server..." << std::endl;

    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);
    CloseHandle(hReadyEvent);
    CloseHandle(hMessageEvent);

    std::cout << "Server terminated." << std::endl;
    return 0;
}