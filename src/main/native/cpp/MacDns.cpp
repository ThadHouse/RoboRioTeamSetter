#include <dns_sd.h>
#include "DnsFinder.h"
#include <iostream>
#include <wpi/SmallString.h>
#include <thread>

struct DnsFinder::Impl {
  DNSServiceRef ServiceRef = nullptr;
  dnssd_sock_t ServiceSock;
  std::thread Thread;

  void ThreadMain() {
    while (true) {
      DNSServiceErrorType error = DNSServiceProcessResult(ServiceRef);
      if (error != kDNSServiceErr_NoError) {
        break;
      }
    }
  }
};

DnsFinder::DnsFinder() : pImpl{std::make_unique<Impl>()} {};
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

  static_cast<DnsFinder*>(context)->OnFound(
      *static_cast<const unsigned int*>(rdata), fullname);
}

static void DnsCompletion(DNSServiceRef sdRef, DNSServiceFlags flags,
                          uint32_t interfaceIndex,
                          DNSServiceErrorType errorCode,
                          const char* serviceName, const char* regtype,
                          const char* replyDomain, void* context) {
  if (errorCode != kDNSServiceErr_NoError) {
    return;
  }

  wpi::SmallString<128> fullname;
  fullname.append(serviceName);
  fullname.push_back('.');
  fullname.append(replyDomain);
  fullname.push_back('\0');

  DNSServiceQueryRecord(&static_cast<DnsFinder*>(context)->pImpl->ServiceRef,
                        flags, interfaceIndex, fullname.c_str(),
                        kDNSServiceType_A, kDNSServiceClass_IN,
                        ServiceQueryRecordReply, context);
}

bool DnsFinder::StartSearch() {
  DNSServiceErrorType status = DNSServiceBrowse(
      &pImpl->ServiceRef, 0, 0, "_ni-rt._tcp", "local", DnsCompletion, this);
  if (status == kDNSServiceErr_NoError) {
    // DNSServiceSetDispatchQueue(pImpl->ServiceRef, dispatch_get_main_queue());
    pImpl->Thread = std::thread([&] { pImpl->ThreadMain(); });
    return true;
  }
  return false;
}

void DnsFinder::StopSearch() {
  DNSServiceRefDeallocate(pImpl->ServiceRef);
  if (pImpl->Thread.joinable())
    pImpl->Thread.join();
}