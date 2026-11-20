#include "common/http.h"

#include <cstdio>
#include <cstring>
#include <iterator>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace nf::http {

std::string Encode(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += char(c);
        } else {
            char b[4];
            std::snprintf(b, sizeof b, "%%%02X", c);
            out += b;
        }
    }
    return out;
}

#ifdef _WIN32

namespace {
std::wstring Wide(const std::string& s) {
    std::wstring w(s.size(), L'\0');
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), int(w.size()));
    w.resize(n > 0 ? size_t(n) : 0);
    return w;
}
}  // namespace

int Request(const char* method, const std::string& url, const std::string& body, std::string& reply, int timeout_ms) {
    reply.clear();
    std::wstring wurl = Wide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {}, path[1024] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = DWORD(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = DWORD(std::size(path));
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) return 0;
    HINTERNET session = WinHttpOpen(L"NightFire", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return 0;
    WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    int status = 0;
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, Wide(method).c_str(), path, nullptr,
                                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                                   : nullptr;
    const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (request &&
        WinHttpSendRequest(request, body.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers, body.empty() ? 0 : DWORD(-1L),
                           body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), DWORD(body.size()),
                           DWORD(body.size()), 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD code = 0, size = sizeof(code);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &code, &size, WINHTTP_NO_HEADER_INDEX))
            status = int(code);
        char b[4096];
        DWORD got = 0;
        while (reply.size() < (1u << 20) && WinHttpReadData(request, b, sizeof b, &got) && got) reply.append(b, got);
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return status;
}

#else

int Request(const char* method, const std::string& url, const std::string& body, std::string& reply, int timeout_ms) {
    reply.clear();
    if (url.rfind("http://", 0) != 0) return 0;  // https needs the Windows build's WinHTTP
    std::string rest = url.substr(7), path = "/";
    if (size_t slash = rest.find('/'); slash != std::string::npos) path = rest.substr(slash), rest.resize(slash);
    std::string host = rest, port = "80";
    if (size_t colon = rest.rfind(':'); colon != std::string::npos) host = rest.substr(0, colon), port = rest.substr(colon + 1);
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) return 0;
    int s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (s >= 0) {
        ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    }
    bool connected = s >= 0 && ::connect(s, res->ai_addr, res->ai_addrlen) == 0;
    ::freeaddrinfo(res);
    if (!connected) {
        if (s >= 0) ::close(s);
        return 0;
    }
    std::string head = std::string(method) + " " + path + " HTTP/1.1\r\nHost: " + rest +
                       "\r\nUser-Agent: NightFire\r\nConnection: close\r\n";
    if (!body.empty())
        head += "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
    head += "\r\n" + body;
    std::string all;
    if (::send(s, head.data(), head.size(), 0) == ssize_t(head.size())) {
        char b[4096];
        for (ssize_t n; all.size() < (1u << 20) && (n = ::recv(s, b, sizeof b, 0)) > 0;) all.append(b, size_t(n));
    }
    ::close(s);
    int status = 0;
    if (std::sscanf(all.c_str(), "HTTP/%*d.%*d %d", &status) != 1) return 0;
    if (size_t end = all.find("\r\n\r\n"); end != std::string::npos) reply = all.substr(end + 4);
    return status;
}

#endif

}  // namespace nf::http
