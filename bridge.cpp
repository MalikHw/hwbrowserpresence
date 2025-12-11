#include <string>
#include <thread>
#include <mutex>
#include <ctime>
#include <iostream>

#ifdef _WIN32
#include "discord-rpc-windows.h"
#else
#include "discord-rpc-linux.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_TYPE SOCKET
#define INVALID_SOCKET_VALUE INVALID_SOCKET
#define close_socket closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#define SOCKET_TYPE int
#define INVALID_SOCKET_VALUE -1
#define close_socket close
#endif

#define WS_PORT 8947
#define APP_ID "1448712260632707153"

std::mutex presenceMutex;
std::string currentTitle = "";
std::string currentDomain = "";
std::string currentFavicon = "";
bool running = true;

#ifdef _WIN32
HWND g_hwnd = NULL;
NOTIFYICONDATA nid = {};

void ShowAboutDialog() {
    ShellExecuteA(NULL, "open", "https://youtube.com/@MalikHw47", NULL, NULL, SW_SHOWNORMAL);
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_USER + 1:
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                AppendMenuA(hMenu, MF_STRING, 1, "About");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuA(hMenu, MF_STRING, 2, "Quit");
                
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                
                if (cmd == 1) {
                    ShowAboutDialog();
                } else if (cmd == 2) {
                    running = false;
                    PostQuitMessage(0);
                }
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void InitTrayIcon() {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "HwBrowserPresenceTray";
    RegisterClassExA(&wc);
    
    g_hwnd = CreateWindowExA(0, "HwBrowserPresenceTray", "HwBrowserPresence", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    
    // Load icon from executable
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    if (!nid.hIcon) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    strcpy_s(nid.szTip, "HwBrowserPresence");
    Shell_NotifyIcon(NIM_ADD, &nid);
}
#endif

void UpdateDiscordPresence() {
    std::lock_guard<std::mutex> lock(presenceMutex);
    
    if (currentTitle.empty()) return;
    
    DiscordRichPresence presence = {};
    presence.details = currentTitle.c_str();
    presence.state = currentDomain.c_str();
    presence.startTimestamp = time(nullptr);
    
    // Use favicon as large image if available
    if (!currentFavicon.empty()) {
        presence.largeImageKey = currentFavicon.c_str();
    } else {
        presence.largeImageKey = "browser";
    }
    presence.largeImageText = currentDomain.c_str();
    
    Discord_UpdatePresence(&presence);
}

std::string base64_decode(const std::string& encoded) {
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded) {
        if (c == '=') break;
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

void HandleWebSocketMessage(const std::string& data) {
    // Simple JSON parsing for our specific format
    size_t titlePos = data.find("\"title\":\"");
    size_t domainPos = data.find("\"domain\":\"");
    size_t faviconPos = data.find("\"favicon\":\"");
    
    if (titlePos != std::string::npos && domainPos != std::string::npos) {
        titlePos += 9;
        size_t titleEnd = data.find("\"", titlePos);
        
        domainPos += 10;
        size_t domainEnd = data.find("\"", domainPos);
        
        if (titleEnd != std::string::npos && domainEnd != std::string::npos) {
            std::lock_guard<std::mutex> lock(presenceMutex);
            currentTitle = data.substr(titlePos, titleEnd - titlePos);
            currentDomain = data.substr(domainPos, domainEnd - domainPos);
            
            // Extract favicon if present
            if (faviconPos != std::string::npos) {
                faviconPos += 11;
                size_t faviconEnd = data.find("\"", faviconPos);
                if (faviconEnd != std::string::npos) {
                    currentFavicon = data.substr(faviconPos, faviconEnd - faviconPos);
                }
            } else {
                currentFavicon = "";
            }
            
            UpdateDiscordPresence();
        }
    }
}

void WebSocketServer() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    SOCKET_TYPE serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET_VALUE) return;
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(WS_PORT);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close_socket(serverSocket);
        return;
    }
    
    listen(serverSocket, 1);
    
    while (running) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        SOCKET_TYPE clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket == INVALID_SOCKET_VALUE) continue;
        
        // WebSocket handshake
        char buffer[4096];
        int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            std::string request(buffer);
            
            size_t keyPos = request.find("Sec-WebSocket-Key: ");
            if (keyPos != std::string::npos) {
                keyPos += 19;
                size_t keyEnd = request.find("\r\n", keyPos);
                std::string key = request.substr(keyPos, keyEnd - keyPos);
                
                std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                                      "Upgrade: websocket\r\n"
                                      "Connection: Upgrade\r\n"
                                      "Sec-WebSocket-Accept: " + key + "\r\n\r\n";
                send(clientSocket, response.c_str(), response.length(), 0);
                
                // Read WebSocket frames
                while (running) {
                    unsigned char frameHeader[2];
                    if (recv(clientSocket, (char*)frameHeader, 2, 0) != 2) break;
                    
                    bool masked = (frameHeader[1] & 0x80) != 0;
                    int payloadLen = frameHeader[1] & 0x7F;
                    
                    if (payloadLen == 126) {
                        unsigned char lenBytes[2];
                        recv(clientSocket, (char*)lenBytes, 2, 0);
                        payloadLen = (lenBytes[0] << 8) | lenBytes[1];
                    }
                    
                    unsigned char mask[4] = {0};
                    if (masked) {
                        recv(clientSocket, (char*)mask, 4, 0);
                    }
                    
                    std::string payload;
                    payload.resize(payloadLen);
                    int totalReceived = 0;
                    while (totalReceived < payloadLen) {
                        int n = recv(clientSocket, &payload[totalReceived], payloadLen - totalReceived, 0);
                        if (n <= 0) break;
                        totalReceived += n;
                    }
                    
                    if (masked) {
                        for (int i = 0; i < payloadLen; i++) {
                            payload[i] ^= mask[i % 4];
                        }
                    }
                    
                    HandleWebSocketMessage(payload);
                }
            }
        }
        
        close_socket(clientSocket);
    }
    
    close_socket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}

int main() {
#ifdef _WIN32
    // Hide console window
    FreeConsole();
    InitTrayIcon();
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    
    DiscordEventHandlers handlers = {};
    Discord_Initialize(APP_ID, &handlers, 1, nullptr);
    
    std::thread wsThread(WebSocketServer);
    
#ifdef _WIN32
    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        Discord_RunCallbacks();
    }
#else
    while (running) {
        Discord_RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif
    
    Discord_Shutdown();
    running = false;
    wsThread.join();
    
    return 0;
}
