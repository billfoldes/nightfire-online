#include "common/upnp.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "core/log.h"

namespace nf::upnp {
namespace {

constexpr int kSearchMs = 2500, kHttpMs = 4000;
constexpr int kLease = 3600, kRenewS = 1800;  // seconds: the router forgets it an hour after the game is gone

struct Url {
    std::string host, path = "/";
    uint16_t port = 80;
};

bool ParseUrl(const std::string& text, Url& out) {
    if (text.compare(0, 7, "http://") != 0) return false;
    size_t slash = text.find('/', 7);
    std::string at = text.substr(7, slash == std::string::npos ? std::string::npos : slash - 7);
    out.path = slash == std::string::npos ? "/" : text.substr(slash);
    out.port = 80;
    if (size_t colon = at.rfind(':'); colon != std::string::npos) {
        int p = std::atoi(at.c_str() + colon + 1);
        if (p <= 0 || p > 65535) return false;
        out.port = uint16_t(p), at.resize(colon);
    }
    out.host = at;
    return !at.empty();
}

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(uint8_t(c)));
    return s;
}

bool Wait(SOCKET s, bool write, int ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return ::select(int(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv) > 0;
}

// a whole HTTP exchange (the router's description, a SOAP call); the status, 0 when nothing answered. local: this
// PC's address as the router sees it
int Http(const Url& u, const char* method, const std::string& headers, const std::string& body, std::string& reply,
         std::string& local) {
    addrinfo hints{};
    hints.ai_family = AF_INET, hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(u.host.c_str(), nullptr, &hints, &res) != 0 || !res) return 0;
    sockaddr_in to = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    ::freeaddrinfo(res);
    to.sin_port = ::htons(u.port);
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;
    u_long one = 1;
    ::ioctlsocket(s, FIONBIO, &one);
    int status = 0;
    if (::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof to) == 0 ||
        (::WSAGetLastError() == WSAEWOULDBLOCK && Wait(s, true, kHttpMs))) {
        int err = 0, len = sizeof err;
        ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        sockaddr_in me{};
        int me_len = sizeof me;
        if (!err && ::getsockname(s, reinterpret_cast<sockaddr*>(&me), &me_len) == 0) {
            char ip[INET_ADDRSTRLEN] = {};
            ::inet_ntop(AF_INET, &me.sin_addr, ip, sizeof ip);
            local = ip;
            char head[512];
            std::snprintf(head, sizeof head, "%s %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\nContent-Length: %zu\r\n",
                          method, u.path.c_str(), u.host.c_str(), unsigned(u.port), body.size());
            std::string out = head + headers + "\r\n" + body;
            size_t sent = 0;
            while (sent < out.size() && Wait(s, true, kHttpMs)) {
                int r = ::send(s, out.data() + sent, int(out.size() - sent), 0);
                if (r <= 0) break;
                sent += size_t(r);
            }
            std::string in;
            char b[4096];
            while (sent == out.size() && in.size() < (256u << 10) && Wait(s, false, kHttpMs)) {
                int r = ::recv(s, b, sizeof b, 0);
                if (r <= 0) break;
                in.append(b, size_t(r));
            }
            size_t end = in.find("\r\n\r\n");
            if (in.compare(0, 5, "HTTP/") == 0 && end != std::string::npos) {
                if (size_t sp = in.find(' '); sp != std::string::npos) status = std::atoi(in.c_str() + sp + 1);
                std::string head_in = Lower(in.substr(0, end));
                reply = in.substr(end + 4);
                if (head_in.find("transfer-encoding: chunked") != std::string::npos) {
                    std::string whole;
                    for (size_t at = 0;;) {
                        size_t line = reply.find("\r\n", at);
                        if (line == std::string::npos) break;
                        size_t n = std::strtoul(reply.c_str() + at, nullptr, 16);
                        if (!n || line + 2 + n > reply.size()) break;
                        whole.append(reply, line + 2, n);
                        at = line + 2 + n + 2;
                    }
                    reply = whole;
                }
            }
        }
    }
    ::closesocket(s);
    return status;
}

// SSDP: the routers on this network that answer for an Internet Gateway Device; their description's URLs
std::vector<std::string> Search() {
    std::vector<std::string> found;
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return found;
    int ttl = 2;
    ::setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof ttl);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = ::htons(1900);
    ::inet_pton(AF_INET, "239.255.255.250", &to.sin_addr);
    for (const char* st : {"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
                           "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
                           "urn:schemas-upnp-org:service:WANIPConnection:1"}) {
        char m[256];
        int n = std::snprintf(m, sizeof m,
                              "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
                              "ST: %s\r\n\r\n", st);
        ::sendto(s, m, n, 0, reinterpret_cast<const sockaddr*>(&to), sizeof to);
    }
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(kSearchMs);
    char b[2048];
    while (true) {
        int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count());
        if (left <= 0 || !Wait(s, false, left)) break;
        sockaddr_in from{};
        int len = sizeof from;
        int n = ::recvfrom(s, b, sizeof b - 1, 0, reinterpret_cast<sockaddr*>(&from), &len);
        if (n <= 0) continue;
        std::string text(b, size_t(n)), low = Lower(text);
        size_t at = low.find("\r\nlocation:");
        if (at == std::string::npos) continue;
        at += 11;
        size_t eol = text.find("\r\n", at);
        std::string url = text.substr(at, eol == std::string::npos ? std::string::npos : eol - at);
        url.erase(0, url.find_first_not_of(" \t"));
        url.erase(url.find_last_not_of(" \t") + 1);
        if (!url.empty() && std::find(found.begin(), found.end(), url) == found.end()) found.push_back(url);
    }
    ::closesocket(s);
    return found;
}

std::string Tag(const std::string& xml, const char* name, size_t from = 0, size_t to = std::string::npos) {
    std::string open = std::string("<") + name + ">", close = std::string("</") + name + ">";
    size_t a = xml.find(open, from);
    if (a == std::string::npos || a >= to) return {};
    a += open.size();
    size_t b = xml.find(close, a);
    if (b == std::string::npos || b > to) return {};
    std::string v = xml.substr(a, b - a);
    v.erase(0, v.find_first_not_of(" \t\r\n"));
    v.erase(v.find_last_not_of(" \t\r\n") + 1);
    return v;
}

struct Service {
    std::string type, control;
};

// the description's port-forwarding service and its control URL, made whole against the description's own
bool FindService(const std::string& xml, const std::string& location, Service& out) {
    for (const char* type : {"urn:schemas-upnp-org:service:WANIPConnection:2",
                             "urn:schemas-upnp-org:service:WANIPConnection:1",
                             "urn:schemas-upnp-org:service:WANPPPConnection:1"}) {
        size_t at = xml.find(std::string(">") + type + "<");
        if (at == std::string::npos) continue;
        size_t end = xml.find("</service>", at);
        std::string control = Tag(xml, "controlURL", at, end);
        if (control.empty()) continue;
        if (control.compare(0, 7, "http://") != 0) {
            std::string base = Tag(xml, "URLBase");
            if (base.empty()) base = location.substr(0, location.find('/', 7));
            while (!base.empty() && base.back() == '/') base.pop_back();
            control = base + (control[0] == '/' ? "" : "/") + control;
        }
        out = {type, control};
        return true;
    }
    return false;
}

// the call's status, and the router's errorCode in a fault (0 for none)
int Soap(const Service& svc, const char* action, const std::string& args, std::string& local, int& fault) {
    Url u;
    fault = 0;
    if (!ParseUrl(svc.control, u)) return 0;
    std::string body = std::string("<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                   "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:") +
                       action + " xmlns:u=\"" + svc.type + "\">" + args + "</u:" + action + "></s:Body></s:Envelope>\r\n";
    std::string headers = "Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"" + svc.type + "#" + action + "\"\r\n";
    std::string reply;
    int status = Http(u, "POST", headers, body, reply, local);
    if (status != 200) fault = std::atoi(Tag(reply, "errorCode").c_str());
    return status;
}

std::string Add(uint16_t port, const std::string& local, int lease) {
    char b[512];
    std::snprintf(b, sizeof b,
                  "<NewRemoteHost></NewRemoteHost><NewExternalPort>%u</NewExternalPort><NewProtocol>UDP</NewProtocol>"
                  "<NewInternalPort>%u</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled>"
                  "<NewPortMappingDescription>NightFire Online</NewPortMappingDescription>"
                  "<NewLeaseDuration>%d</NewLeaseDuration>",
                  unsigned(port), unsigned(port), local.c_str(), lease);
    return b;
}

// one try at every router found: true once one forwards the port. quiet: the outcome is not logged
bool Forward(uint16_t port, bool quiet) {
    std::vector<std::string> routers;
    if (const char* at = std::getenv("NF_NET_UPNP_AT"); at && *at) routers.push_back(at);
    else routers = Search();
    if (routers.empty()) {
        if (!quiet)
            LOGR("net: no router answered UPnP (it may have it switched off): players on the internet need UDP %u "
                 "forwarded to this PC by hand", unsigned(port));
        return false;
    }
    std::string why = "no router offered port forwarding";
    for (const std::string& location : routers) {
        Url u;
        std::string xml, local;
        if (!ParseUrl(location, u) || Http(u, "GET", "", "", xml, local) != 200) {
            why = "the router's description did not come";
            continue;
        }
        Service svc;
        if (!FindService(xml, location, svc)) continue;
        int fault = 0;  // local: this PC's address on the connection to the router, the one it forwards to
        int status = Soap(svc, "AddPortMapping", Add(port, local, kLease), local, fault);
        if (status != 200 && fault == 725)  // OnlyPermanentLeasesSupported
            status = Soap(svc, "AddPortMapping", Add(port, local, 0), local, fault);
        if (status == 200) {
            if (!quiet)
                LOGR("net: the router forwards UDP %u to this PC now (UPnP, asked again every %d minutes while hosting)",
                     unsigned(port), kRenewS / 60);
            return true;
        }
        char b[96];
        if (fault == 718)
            std::snprintf(b, sizeof b, "another PC on this network has UDP %u forwarded already", unsigned(port));
        else if (fault)
            std::snprintf(b, sizeof b, "the router refused (UPnP error %d)", fault);
        else
            std::snprintf(b, sizeof b, "the router did not answer the request (HTTP %d)", status);
        why = b;
    }
    if (!quiet)
        LOGR("net: UPnP did not forward UDP %u (%s): players on the internet need it forwarded to this PC by hand",
             unsigned(port), why.c_str());
    return false;
}

}  // namespace

void Open(uint16_t port) {
    std::thread([port] {
        bool ok = Forward(port, false);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(ok ? kRenewS : 300));
            bool again = Forward(port, true);
            if (ok && !again) LOGR("net: the router stopped answering UPnP; UDP %u may not be forwarded any more", unsigned(port));
            if (!ok && again) LOGR("net: the router forwards UDP %u to this PC now (UPnP)", unsigned(port));
            ok = again;
        }
    }).detach();
}

}  // namespace nf::upnp
