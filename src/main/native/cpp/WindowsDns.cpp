#if _WIN32
#define UNICODE
#include <Windows.h>
#include <WinDNS.h>

#include "DnsFinder.h"
#include "wpi/ConvertUTF.h"
#include <wpi/SmallString.h>
#include <iostream>

#pragma comment(lib, "dnsapi.lib")

struct DnsFinder::Impl {
  DNS_SERVICE_CANCEL ServiceCancel;
};

DnsFinder::DnsFinder() : pImpl{std::make_unique<Impl>()} {};
DnsFinder::~DnsFinder() {}

static _Function_class_(DNS_QUERY_COMPLETION_ROUTINE) VOID WINAPI
    DnsCompletion(_In_ PVOID pQueryContext,
                  _Inout_ PDNS_QUERY_RESULT pQueryResults) {
  DNS_RECORDW* current = pQueryResults->pQueryRecords;
  while (current != nullptr) {
    if (current->wType == DNS_TYPE_A) {
      IP4_ADDRESS rawIp = current->Data.A.IpAddress;

      wpi::SmallString<128> name;
      wpi::span<const wpi::UTF16> wideName{reinterpret_cast<const wpi::UTF16*>(current->pName),
                                           wcslen(current->pName)};
      wpi::convertUTF16ToUTF8String(wideName, name);
      static_cast<DnsFinder*>(pQueryContext)->OnFound(rawIp, name);
    }
    current = current->pNext;
  }
  DnsFree(pQueryResults->pQueryRecords, DNS_FREE_TYPE::DnsFreeRecordList);
}

bool DnsFinder::StartSearch() {
  DNS_SERVICE_BROWSE_REQUEST request;
  request.InterfaceIndex = 0;
  request.pQueryContext = this;
  request.QueryName = L"_ni-rt._tcp.local";
  request.Version = 2;
  request.pBrowseCallbackV2 = DnsCompletion;
  DNS_STATUS status = DnsServiceBrowse(
      &request, &pImpl->ServiceCancel);
  return status == DNS_REQUEST_PENDING;
}

void DnsFinder::StopSearch() {
  DnsServiceBrowseCancel(&pImpl->ServiceCancel);
}
#endif
