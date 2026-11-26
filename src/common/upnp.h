// A host by its IP address asks the router to forward its UDP port (UPnP's Internet Gateway Device: SSDP finds the
// router, its description names the WANIPConnection service, AddPortMapping asks it). Plain sockets, so the harness
// runs it too
#pragma once

#include <cstdint>

namespace nf::upnp {

// in the background: forwards UDP port to this PC, again every half hour while the game runs; the log says how it
// went. NF_NET_UPNP_AT=<description url> skips the search (the harness's fake router)
void Open(uint16_t port);

}  // namespace nf::upnp
