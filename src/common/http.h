// A small blocking HTTP client for the internet game list (net.cpp). Windows: WinHTTP, http and https. Linux (the
// headless harness): http only
#pragma once

#include <string>

namespace nf::http {

// method "GET" or "POST" (body sent as a form); the reply's body; the status code, 0 when nothing answered
int Request(const char* method, const std::string& url, const std::string& body, std::string& reply,
            int timeout_ms = 5000);

// a form field's value, percent-encoded
std::string Encode(const std::string& text);

}  // namespace nf::http
