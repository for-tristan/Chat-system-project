// ChatServer.cpp - Socket Chat Server with GUI (FIXED VERSION)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <commctrl.h>
#include <cstdio>  // Added for sprintf_s

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

// Constants - Converted to constexpr
constexpr int IDC_LOG_EDIT = 101;
constexpr int IDC_CLIENT_LIST = 102;
constexpr int IDC_PORT_EDIT = 103;
constexpr int IDC_START_BTN = 104;
constexpr int IDC_STOP_BTN = 105;
constexpr int IDC_CLEAR_BTN = 106;
constexpr int IDC_PORT_STATIC = 107;
constexpr int IDC_STATUS_STATIC = 108;

// Global Variables
HWND hLogEdit, hClientList, hPortEdit, hStartBtn, hStopBtn, hStatusText, hClearBtn;
HINSTANCE hInst;
SOCKET serverSocket = INVALID_SOCKET;
std::vector<SOCKET> clientSockets;
std::mutex clientMutex;
bool serverRunning = false;
std::thread serverThread;

// Function Prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeControls(HWND hwnd);
void LogMessage(const std::string& message);
void UpdateClientList();
void StartServer();
void StopServer();
static void ServerThread(int port);  // Made static

// Windows Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow) {

    hInst = hInstance;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // Register Window Class
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ChatServerClass";
    wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    // Create Window
    HWND hwnd = CreateWindowEx(
        0,
        L"ChatServerClass",
        L"Chat Server",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message Loop
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitializeControls(hwnd);
        LogMessage("Chat Server initialized.");
        LogMessage("Set port and click 'Start Server'.");
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_START_BTN) {
            StartServer();
        }
        else if (LOWORD(wParam) == IDC_STOP_BTN) {
            StopServer();
        }
        else if (LOWORD(wParam) == IDC_CLEAR_BTN) {
            SetWindowText(hLogEdit, L"");
        }
        break;

    case WM_CLOSE:
        StopServer();
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Initialize GUI Controls
void InitializeControls(HWND hwnd) {
    // Status Label
    hStatusText = CreateWindow(L"STATIC", L"Status: Stopped",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 200, 25, hwnd, (HMENU)IDC_STATUS_STATIC, hInst, NULL);

    // Port Label
    CreateWindow(L"STATIC", L"Port:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        230, 22, 40, 25, hwnd, (HMENU)IDC_PORT_STATIC, hInst, NULL);

    // Port Edit Box
    hPortEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"54000",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        275, 20, 80, 25, hwnd, (HMENU)IDC_PORT_EDIT, hInst, NULL);

    // Start Button
    hStartBtn = CreateWindow(L"BUTTON", L"Start Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 20, 100, 25, hwnd, (HMENU)IDC_START_BTN, hInst, NULL);

    // Stop Button
    hStopBtn = CreateWindow(L"BUTTON", L"Stop Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        480, 20, 100, 25, hwnd, (HMENU)IDC_STOP_BTN, hInst, NULL);

    // Connected Clients Label
    CreateWindow(L"STATIC", L"Connected Clients:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 60, 150, 25, hwnd, NULL, hInst, NULL);

    // Client List Box
    hClientList = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        20, 85, 200, 200, hwnd, (HMENU)IDC_CLIENT_LIST, hInst, NULL);

    // Server Log Label
    CreateWindow(L"STATIC", L"Server Log:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        240, 60, 100, 25, hwnd, NULL, hInst, NULL);

    // Log Edit Box (Multi-line, read-only)
    hLogEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        240, 85, 340, 200, hwnd, (HMENU)IDC_LOG_EDIT, hInst, NULL);

    // Clear Log Button
    hClearBtn = CreateWindow(L"BUTTON", L"Clear Log",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        240, 295, 100, 25, hwnd, (HMENU)IDC_CLEAR_BTN, hInst, NULL);

    // Set Font
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SendMessage(hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hClientList, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPortEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hStatusText, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// Log Message to GUI
void LogMessage(const std::string& message) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[64];
    // FIXED: Use proper sprintf_s signature
    sprintf_s(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    std::string fullMessage = timestamp + message + "\r\n";

    // Convert to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), (int)fullMessage.length(), NULL, 0);
    std::vector<wchar_t> wideMsg(wideLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), (int)fullMessage.length(), wideMsg.data(), wideLen);
    wideMsg[wideLen] = 0;

    // Append to log
    int len = GetWindowTextLength(hLogEdit);
    SendMessage(hLogEdit, EM_SETSEL, len, len);
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)wideMsg.data());

    // Scroll to bottom
    SendMessage(hLogEdit, EM_LINESCROLL, 0, 1000);
}

// Update Client List Display
void UpdateClientList() {
    SendMessage(hClientList, LB_RESETCONTENT, 0, 0);

    std::lock_guard<std::mutex> lock(clientMutex);
    for (size_t i = 0; i < clientSockets.size(); i++) {
        wchar_t clientInfo[64];
        // FIXED: Added proper capture for i
        int socketValue = (int)clientSockets[i];
        swprintf_s(clientInfo, 64, L"Client %zu (Socket: %d)", i + 1, socketValue);
        SendMessage(hClientList, LB_ADDSTRING, 0, (LPARAM)clientInfo);
    }
}

// Start Server Function
void StartServer() {
    if (serverRunning) return;

    // Get port from edit box
    wchar_t portText[32];
    GetWindowText(hPortEdit, portText, 32);
    int port = _wtoi(portText);
    if (port <= 0 || port > 65535) {
        MessageBox(NULL, L"Please enter a valid port (1-65535)", L"Invalid Port", MB_OK | MB_ICONWARNING);
        return;
    }

    // Disable start button, enable stop button
    EnableWindow(hStartBtn, FALSE);
    EnableWindow(hStopBtn, TRUE);
    EnableWindow(hPortEdit, FALSE);

    // Update status
    SetWindowText(hStatusText, L"Status: Starting...");

    // Start server in separate thread
    serverRunning = true;
    serverThread = std::thread(ServerThread, port);
    serverThread.detach();
}

// Stop Server Function
void StopServer() {
    if (!serverRunning) return;

    serverRunning = false;

    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        for (SOCKET s : clientSockets) {
            closesocket(s);
        }
        clientSockets.clear();
    }

    // Close server socket
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    // Update GUI
    EnableWindow(hStartBtn, TRUE);
    EnableWindow(hStopBtn, FALSE);
    EnableWindow(hPortEdit, TRUE);
    SetWindowText(hStatusText, L"Status: Stopped");

    LogMessage("Server stopped.");
    UpdateClientList();
}

// Server Thread Function (Made static)
static void ServerThread(int port) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogMessage("WSAStartup failed!");
        return;
    }

    // Create server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        LogMessage("Failed to create socket!");
        WSACleanup();
        return;
    }

    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg), "Bind failed! Error: %d", WSAGetLastError());
        LogMessage(errorMsg);
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // Listen for connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        LogMessage("Listen failed!");
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // Log success
    char logMsg[256];
    sprintf_s(logMsg, sizeof(logMsg), "Server started on port %d", port);
    LogMessage(logMsg);
    SetWindowText(hStatusText, L"Status: Running");

    // Accept client connections
    while (serverRunning) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(0, &readSet, NULL, NULL, &timeout);

        if (activity > 0 && FD_ISSET(serverSocket, &readSet)) {
            sockaddr_in clientAddr;
            int clientSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);

            if (clientSocket == INVALID_SOCKET) {
                LogMessage("Accept failed!");
                continue;
            }

            // Add to client list
            {
                std::lock_guard<std::mutex> lock(clientMutex);
                clientSockets.push_back(clientSocket);
            }

            // Log connection
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            sprintf_s(logMsg, sizeof(logMsg), "Client connected: %s:%d", clientIP, ntohs(clientAddr.sin_port));
            LogMessage(logMsg);

            UpdateClientList();

            // FIXED: Proper lambda capture for thread
            std::thread clientHandler([clientSocket, clientAddr]() {
                char buffer[1024];
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

                // Capture these variables by value
                bool running = serverRunning;
                auto& socketsRef = clientSockets;
                auto& mutexRef = clientMutex;

                while (running) {
                    memset(buffer, 0, sizeof(buffer));
                    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

                    if (bytesReceived <= 0) {
                        // Client disconnected
                        break;
                    }

                    // Log received message
                    char msg[1200];
                    sprintf_s(msg, sizeof(msg), "%s: %s", clientIP, buffer);
                    LogMessage(msg);

                    // Broadcast to all other clients
                    std::lock_guard<std::mutex> lock(mutexRef);
                    for (SOCKET s : socketsRef) {
                        if (s != clientSocket) {
                            send(s, buffer, bytesReceived, 0);
                        }
                    }

                    // Update running flag
                    running = serverRunning;
                }

                // Remove client
                {
                    std::lock_guard<std::mutex> lock(mutexRef);
                    auto it = std::find(socketsRef.begin(), socketsRef.end(), clientSocket);
                    if (it != socketsRef.end()) {
                        socketsRef.erase(it);
                    }
                }

                closesocket(clientSocket);

                char disconnectMsg[256];
                sprintf_s(disconnectMsg, sizeof(disconnectMsg), "Client disconnected: %s", clientIP);
                LogMessage(disconnectMsg);
                UpdateClientList();
                });

            clientHandler.detach();
        }
    }

    WSACleanup();
}