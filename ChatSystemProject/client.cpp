// SharedMemoryClient.cpp
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

int main() {
    std::cout << "=== SHARED MEMORY CHAT CLIENT ===" << std::endl;
    std::cout << "Connecting to server..." << std::endl;

    // 1. Open existing shared memory
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,    // Read/write access
        FALSE,                  // Do not inherit the name
        SHM_NAME                // Name of mapping object
    );

    if (hMapFile == NULL) {
        std::cerr << "Could not open file mapping object: " << GetLastError() << std::endl;
        std::cerr << "Make sure the server is running first!" << std::endl;
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
        std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // 3. Open mutex
    HANDLE hMutex = OpenMutex(
        MUTEX_ALL_ACCESS,       // Request full access
        FALSE,                  // Not inheritable
        MUTEX_NAME              // Mutex name
    );

    if (hMutex == NULL) {
        std::cerr << "OpenMutex error: " << GetLastError() << std::endl;
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        return 1;
    }

    // 4. Open events
    HANDLE hReadyEvent = OpenEvent(
        EVENT_ALL_ACCESS,       // Request full access
        FALSE,                  // Not inheritable
        READY_EVENT_NAME        // Event name
    );

    HANDLE hMessageEvent = OpenEvent(
        EVENT_ALL_ACCESS,       // Request full access
        FALSE,                  // Not inheritable
        MESSAGE_EVENT_NAME      // Event name
    );

    // 5. Signal client connection
    WaitForSingleObject(hMutex, INFINITE);
    pData->clientConnected = true;
    strcpy_s(pData->message, "Client connected!");
    ReleaseMutex(hMutex);

    std::cout << "Connected to server!" << std::endl;
    std::cout << "\n=== CHAT STARTED ===" << std::endl;
    std::cout << "Type your messages (type 'exit' to quit):" << std::endl;

    // Thread to receive messages
    std::thread receiver([&]() {
        while (pData->serverRunning) {
            // Wait for message from server
            DWORD waitResult = WaitForSingleObject(hMessageEvent, 100);

            if (waitResult == WAIT_OBJECT_0) {
                WaitForSingleObject(hMutex, INFINITE);

                if (strlen(pData->message) > 0 && pData->clientConnected) {
                    // Message from server
                    std::cout << "\n[Server]: " << pData->message << std::endl;
                    std::cout << "[Client]: ";

                    // Clear message and reset event
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
        std::cout << "[Client]: ";
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }

        WaitForSingleObject(hMutex, INFINITE);
        strcpy_s(pData->message, input.c_str());
        pData->clientConnected = false;  // Flag that client sent message
        ReleaseMutex(hMutex);

        // Signal server
        SetEvent(hMessageEvent);
    }

    // Cleanup
    receiver.join();

    std::cout << "Disconnecting..." << std::endl;

    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);
    CloseHandle(hReadyEvent);
    CloseHandle(hMessageEvent);

    std::cout << "Client terminated." << std::endl;
    return 0;
}