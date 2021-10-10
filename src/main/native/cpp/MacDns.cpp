#include <dns_sd.h>
#include "DnsFinder.h"
#include <iostream>
#include <wpi/SmallString.h>

struct DnsFinder::Impl {
  DNSServiceRef ServiceRef = nullptr;
  dnssd_sock_t ServiceSock;
};

DnsFinder::DnsFinder() : pImpl{std::make_unique<Impl>()} {};
DnsFinder::~DnsFinder() {}

static
void ServiceQueryRecordReply
(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char                          *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    uint16_t rdlen,
    const void                          *rdata,
    uint32_t ttl,
    void                                *context
) {
  if (rdlen != 4 || rrtype != kDNSServiceType_A) {
    return;
  }

  static_cast<DnsFinder*>(context)->OnFound(*static_cast<const unsigned int*>(rdata), fullname);
  DNSServiceSetDispatchQueue(static_cast<DnsFinder*>(context)->pImpl->ServiceRef, dispatch_get_main_queue());

  // TODO
  printf("IP resolved %s %d\n", fullname, rdlen);
   fflush(stdout); 
}

static void DnsCompletion(DNSServiceRef sdRef, DNSServiceFlags flags,
                          uint32_t interfaceIndex,
                          DNSServiceErrorType errorCode,
                          const char* serviceName, const char* regtype,
                          const char* replyDomain, void* context) {
  if (errorCode != kDNSServiceErr_NoError) {
    printf("Error %d\n", errorCode);
    fflush(stdout);
    return;
  }

  wpi::SmallString<128> fullname;
  //fullname.append("roboRIO-9998-FRC.local.");
  fullname.append(serviceName);
  fullname.push_back('.');
  // fullname.append(regtype);
  fullname.append(replyDomain);
  fullname.push_back('\0');

  printf("%s\n", fullname.c_str());

  DNSServiceErrorType innerError = DNSServiceQueryRecord(&static_cast<DnsFinder*>(context)->pImpl->ServiceRef, flags,
                    interfaceIndex, fullname.c_str(), kDNSServiceType_A, kDNSServiceClass_IN, ServiceQueryRecordReply,
                    context);
                    DNSServiceSetDispatchQueue(static_cast<DnsFinder*>(context)->pImpl->ServiceRef, dispatch_get_main_queue());
  printf("Found %s %s %s %d %d\n", serviceName, regtype, replyDomain, innerError, flags);
  fflush(stdout);
}

bool DnsFinder::StartSearch() {
  DNSServiceErrorType status = DNSServiceBrowse(
      &pImpl->ServiceRef, 0, 0, "_ni-rt._tcp", "local", DnsCompletion, this);
  printf("Status %d\n", status);
  fflush(stdout);
  //DNSServiceProcessResult(pImpl->ServiceRef);
  //
  if (status == kDNSServiceErr_NoError) {
    DNSServiceSetDispatchQueue(pImpl->ServiceRef, dispatch_get_main_queue());
    
    return true;
  }
  return false;
}

void DnsFinder::StopSearch() {
  DNSServiceRefDeallocate(pImpl->ServiceRef);
}