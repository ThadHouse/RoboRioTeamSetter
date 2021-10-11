#ifndef _WIN32

#include <dns_sd.h>
#include "DnsFinder.h"
#include <stdio.h>
#include <wpi/SmallString.h>
#include <thread>
#include <vector>
#include <poll.h>
#include <atomic>

struct DnsResolveState {
  DnsResolveState(DnsFinder* finder) : Finder{finder} {}
  ~DnsResolveState() {
    if (ResolveRef != nullptr) {
      DNSServiceRefDeallocate(ResolveRef);
    }
  }

  DNSServiceRef ResolveRef = nullptr;
  dnssd_sock_t ResolveSocket;

  DnsFinder* Finder;

  std::string MacAddress;
};

struct DnsFinder::Impl {
  DNSServiceRef ServiceRef = nullptr;
  dnssd_sock_t ServiceQuerySocket;
  std::vector<std::unique_ptr<DnsResolveState>> ResolveStates;
  std::thread Thread;
  std::atomic_bool running;

  void ThreadMain() {
    std::vector<pollfd> readSockets;
    std::vector<DNSServiceRef> serviceRefs;
    while (true) {
      readSockets.clear();
      serviceRefs.clear();

      readSockets.emplace_back(pollfd{ServiceQuerySocket, POLLIN, 0});
      serviceRefs.emplace_back(ServiceRef);

      for (auto&& i : ResolveStates) {
        readSockets.emplace_back(pollfd{i->ResolveSocket, POLLIN, 0});
        serviceRefs.emplace_back(i->ResolveRef);
      }

      int res = poll(readSockets.begin().base(), readSockets.size(), 100);

      if (res > 0) {
        for (size_t i = 0; i < readSockets.size(); i++) {
          if (readSockets[i].revents == POLLIN) {
            DNSServiceProcessResult(serviceRefs[i]);
          }
        }
      } else if (res == 0) {
        if (!running) {
          break;
        }
      } else {
        break;
      }
    }
    ResolveStates.clear();
    DNSServiceRefDeallocate(ServiceRef);
  }
};

DnsFinder::DnsFinder() : pImpl{std::make_unique<Impl>()} {}
DnsFinder::~DnsFinder() {}

static void ServiceQueryRecordReply(DNSServiceRef sdRef, DNSServiceFlags flags,
                                    uint32_t interfaceIndex,
                                    DNSServiceErrorType errorCode,
                                    const char* fullname, uint16_t rrtype,
                                    uint16_t rrclass, uint16_t rdlen,
                                    const void* rdata, uint32_t ttl,
                                    void* context) {
  if (rdlen != 4 || rrtype != kDNSServiceType_A) {
    return;
  }

  DnsResolveState* resolveState = static_cast<DnsResolveState*>(context);
  printf("%s\n", resolveState->MacAddress.c_str());
  fflush(stdout);

  resolveState->Finder->OnFound(*static_cast<const unsigned int*>(rdata),
                                fullname);

  resolveState->Finder->pImpl->ResolveStates.erase(std::find_if(
      resolveState->Finder->pImpl->ResolveStates.begin(),
      resolveState->Finder->pImpl->ResolveStates.end(),
      [resolveState](auto& a) { return a.get() == resolveState; }));
}

void ServiceResolveReply(DNSServiceRef sdRef, DNSServiceFlags flags,
                         uint32_t interfaceIndex, DNSServiceErrorType errorCode,
                         const char* fullname, const char* hosttarget,
                         uint16_t port, /* In network byte order */
                         uint16_t txtLen, const unsigned char* txtRecord,
                         void* context) {
  if (errorCode != kDNSServiceErr_NoError) {
    return;
  }

  DnsResolveState* resolveState = static_cast<DnsResolveState*>(context);
  DNSServiceRefDeallocate(resolveState->ResolveRef);
  resolveState->ResolveRef = nullptr;
  resolveState->ResolveSocket = 0;

  resolveState->MacAddress = std::string(txtRecord, txtRecord + txtLen);

  errorCode = DNSServiceQueryRecord(
      &resolveState->ResolveRef, flags, interfaceIndex, hosttarget,
      kDNSServiceType_A, kDNSServiceClass_IN, ServiceQueryRecordReply, context);

  if (errorCode == kDNSServiceErr_NoError) {
    resolveState->ResolveSocket = DNSServiceRefSockFD(resolveState->ResolveRef);
  } else {
    resolveState->Finder->pImpl->ResolveStates.erase(std::find_if(
        resolveState->Finder->pImpl->ResolveStates.begin(),
        resolveState->Finder->pImpl->ResolveStates.end(),
        [resolveState](auto& a) { return a.get() == resolveState; }));
  }
}

static void DnsCompletion(DNSServiceRef sdRef, DNSServiceFlags flags,
                          uint32_t interfaceIndex,
                          DNSServiceErrorType errorCode,
                          const char* serviceName, const char* regtype,
                          const char* replyDomain, void* context) {
  if (errorCode != kDNSServiceErr_NoError) {
    return;
  }
  if (flags != kDNSServiceFlagsAdd) {
    return;
  }

  DnsFinder* finder = static_cast<DnsFinder*>(context);
  auto& resolveState = finder->pImpl->ResolveStates.emplace_back(
      std::make_unique<DnsResolveState>(finder));

  errorCode = DNSServiceResolve(&resolveState->ResolveRef, 0, interfaceIndex,
                                serviceName, regtype, replyDomain,
                                ServiceResolveReply, resolveState.get());

  if (errorCode == kDNSServiceErr_NoError) {
    resolveState->ResolveSocket = DNSServiceRefSockFD(resolveState->ResolveRef);
  } else {
    resolveState->Finder->pImpl->ResolveStates.erase(std::find_if(
        resolveState->Finder->pImpl->ResolveStates.begin(),
        resolveState->Finder->pImpl->ResolveStates.end(),
        [r = resolveState.get()](auto& a) { return a.get() == r; }));
  }
}

bool DnsFinder::StartSearch() {
  setenv("AVAHI_COMPAT_NOWARN", "1", 0);

  DNSServiceErrorType status = DNSServiceBrowse(
      &pImpl->ServiceRef, 0, 0, "_ni-rt._tcp", "local", DnsCompletion, this);
  if (status == kDNSServiceErr_NoError) {
    pImpl->ServiceQuerySocket = DNSServiceRefSockFD(pImpl->ServiceRef);
    pImpl->running = true;
    // DNSServiceSetDispatchQueue(pImpl->ServiceRef, dispatch_get_main_queue());
    pImpl->Thread = std::thread([&] { pImpl->ThreadMain(); });
    return true;
  }
  return false;
}

void DnsFinder::StopSearch() {
  pImpl->running = false;
  if (pImpl->Thread.joinable())
    pImpl->Thread.join();
}

#endif
