// ChatClient.cpp - Socket Chat Client with GUI (FIXED VERSION)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <commctrl.h>
#include <cstdio>  // Added for sprintf_s
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

// Constants - Converted to constexpr
constexpr int IDC_CHAT_EDIT = 201;
constexpr int IDC_INPUT_EDIT = 202;
constexpr int IDC_SEND_BTN = 203;
constexpr int IDC_CONNECT_BTN = 204;
constexpr int IDC_DISCONNECT_BTN = 205;
constexpr int IDC_SERVER_EDIT = 206;
constexpr int IDC_PORT_EDIT = 207;
constexpr int IDC_NAME_EDIT = 208;
constexpr int IDC_STATUS_STATIC = 209;

// Global Variables
HWND hChatEdit, hInputEdit, hSendBtn, hConnectBtn, hDisconnectBtn;
HWND hServerEdit, hPortEdit, hNameEdit, hStatusText;
HINSTANCE hInst;
SOCKET clientSocket = INVALID_SOCKET;
bool connected = false;
std::thread receiveThread;
std::string clientName = "User";

// Function Prototypes
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitializeControls(HWND hwnd);
void AppendChat(const std::string& message);
void ConnectToServer();
void DisconnectFromServer();
void SendMessageToServer();
static void ReceiveMessages();  // Made static

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
    wc.lpszClassName = L"ChatClientClass";
    wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    // Create Window
    HWND hwnd = CreateWindowEx(
        0,
        L"ChatClientClass",
        L"Chat Client",
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
        AppendChat("Chat Client initialized.");
        AppendChat("Enter server details and click 'Connect'.");
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CONNECT_BTN:
            ConnectToServer();
            break;
        case IDC_DISCONNECT_BTN:
            DisconnectFromServer();
            break;
        case IDC_SEND_BTN:
            SendMessageToServer();
            break;
        case IDC_INPUT_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                // Enable send button if there's text
                wchar_t text[256];
                GetWindowText(hInputEdit, text, 256);
                EnableWindow(hSendBtn, wcslen(text) > 0);
            }
            break;
        }
        break;

    case WM_CLOSE:
        DisconnectFromServer();
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
    hStatusText = CreateWindow(L"STATIC", L"Status: Disconnected",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 200, 25, hwnd, (HMENU)IDC_STATUS_STATIC, hInst, NULL);

    // Server Settings Group
    CreateWindow(L"STATIC", L"Server:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        230, 22, 50, 25, hwnd, NULL, hInst, NULL);

    hServerEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
        WS_CHILD | WS_VISIBLE,
        285, 20, 120, 25, hwnd, (HMENU)IDC_SERVER_EDIT, hInst, NULL);

    CreateWindow(L"STATIC", L"Port:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        410, 22, 30, 25, hwnd, NULL, hInst, NULL);

    hPortEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"54000",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        445, 20, 60, 25, hwnd, (HMENU)IDC_PORT_EDIT, hInst, NULL);

    CreateWindow(L"STATIC", L"Name:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        20, 57, 40, 25, hwnd, NULL, hInst, NULL);

    hNameEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"User",
        WS_CHILD | WS_VISIBLE,
        65, 55, 120, 25, hwnd, (HMENU)IDC_NAME_EDIT, hInst, NULL);

    // Connect Button
    hConnectBtn = CreateWindow(L"BUTTON", L"Connect",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        515, 20, 75, 25, hwnd, (HMENU)IDC_CONNECT_BTN, hInst, NULL);

    // Disconnect Button
    hDisconnectBtn = CreateWindow(L"BUTTON", L"Disconnect",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        515, 55, 75, 25, hwnd, (HMENU)IDC_DISCONNECT_BTN, hInst, NULL);

    // Chat Display (Multi-line, read-only)
    hChatEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_READONLY,
        20, 90, 560, 300, hwnd, (HMENU)IDC_CHAT_EDIT, hInst, NULL);

    // Input Field
    hInputEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 400, 480, 30, hwnd, (HMENU)IDC_INPUT_EDIT, hInst, NULL);

    // Send Button
    hSendBtn = CreateWindow(L"BUTTON", L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        510, 400, 70, 30, hwnd, (HMENU)IDC_SEND_BTN, hInst, NULL);

    // Set Font
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SendMessage(hChatEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hServerEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPortEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hNameEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hStatusText, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// Append Message to Chat Display
void AppendChat(const std::string& message) {
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

    // Append to chat
    int len = GetWindowTextLength(hChatEdit);
    SendMessage(hChatEdit, EM_SETSEL, len, len);
    SendMessage(hChatEdit, EM_REPLACESEL, 0, (LPARAM)wideMsg.data());

    // Scroll to bottom
    SendMessage(hChatEdit, EM_LINESCROLL, 0, 1000);
}

// Connect to Server
void ConnectToServer() {
    if (connected) return;

    // Get server details
    wchar_t serverText[256], portText[32], nameText[32];
    GetWindowText(hServerEdit, serverText, 256);
    GetWindowText(hPortEdit, portText, 32);
    GetWindowText(hNameEdit, nameText, 32);

    // Convert to narrow strings
    char server[256], clientNameStr[32];
    WideCharToMultiByte(CP_UTF8, 0, serverText, -1, server, 256, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, nameText, -1, clientNameStr, 32, NULL, NULL);
    clientName = clientNameStr;

    int port = _wtoi(portText);
    if (port <= 0 || port > 65535) {
        MessageBox(NULL, L"Please enter a valid port (1-65535)", L"Invalid Port", MB_OK | MB_ICONWARNING);
        return;
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AppendChat("WSAStartup failed!");
        return;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        AppendChat("Failed to create socket!");
        WSACleanup();
        return;
    }

    // Connect to server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(server);

    AppendChat("Connecting to server...");
    SetWindowText(hStatusText, L"Status: Connecting...");

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        AppendChat("Connection failed! Make sure server is running.");
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    connected = true;

    // Update GUI
    EnableWindow(hConnectBtn, FALSE);
    EnableWindow(hDisconnectBtn, TRUE);
    EnableWindow(hServerEdit, FALSE);
    EnableWindow(hPortEdit, FALSE);
    EnableWindow(hNameEdit, FALSE);
    SetWindowText(hStatusText, L"Status: Connected");

    AppendChat("Connected to server!");
    AppendChat("Type your messages below:");

    // Start receive thread
    receiveThread = std::thread(ReceiveMessages);
    receiveThread.detach();
}

// Disconnect from Server
void DisconnectFromServer() {
    if (!connected) return;

    connected = false;

    // Close socket
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    WSACleanup();

    // Update GUI
    EnableWindow(hConnectBtn, TRUE);
    EnableWindow(hDisconnectBtn, FALSE);
    EnableWindow(hServerEdit, TRUE);
    EnableWindow(hPortEdit, TRUE);
    EnableWindow(hNameEdit, TRUE);
    EnableWindow(hSendBtn, FALSE);
    SetWindowText(hStatusText, L"Status: Disconnected");

    AppendChat("Disconnected from server.");
}

// Send Message to Server
void SendMessageToServer() {
    if (!connected) return;

    wchar_t messageText[1024];
    GetWindowText(hInputEdit, messageText, 1024);

    if (wcslen(messageText) == 0) return;

    // Convert to narrow string
    char message[1024];
    WideCharToMultiByte(CP_UTF8, 0, messageText, -1, message, 1024, NULL, NULL);

    // Format: "Name: message"
    std::string formattedMessage = clientName + ": " + message;

    // Send to server
    if (send(clientSocket, formattedMessage.c_str(), (int)formattedMessage.length(), 0) == SOCKET_ERROR) {
        AppendChat("Failed to send message!");
        return;
    }

    // Clear input field
    SetWindowText(hInputEdit, L"");
    EnableWindow(hSendBtn, FALSE);
}

// Receive Messages Thread (Made static)
static void ReceiveMessages() {
    char buffer[1024];
    bool localConnected = connected;  // Local copy

    while (localConnected) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) {
            // Server disconnected
            if (localConnected) {
                AppendChat("Disconnected from server.");
            }
            break;
        }

        // Display received message
        AppendChat(buffer);

        // Update local flag
        localConnected = connected;
    }

    // Update connection status if needed
    if (connected) {
        connected = false;
        // We need to update GUI from main thread - this is simplified
        // In real app, you'd post a message to the main window
    }
}