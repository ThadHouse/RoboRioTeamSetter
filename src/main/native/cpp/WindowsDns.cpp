#if _WIN32
#define UNICODE
#include <Windows.h>
#include <WinDNS.h>

#include "DnsFinder.h"
#include "wpi/ConvertUTF.h"
#include <wpi/SmallString.h>
#include <string_view>

#pragma comment(lib, "dnsapi.lib")

struct DnsFinder::Impl {
  DNS_SERVICE_CANCEL ServiceCancel;
};

DnsFinder::DnsFinder() : pImpl{std::make_unique<Impl>()} {};
DnsFinder::~DnsFinder() {}

static _Function_class_(DNS_QUERY_COMPLETION_ROUTINE) VOID WINAPI
    DnsCompletion(_In_ PVOID pQueryContext,
                  _Inout_ PDNS_QUERY_RESULT pQueryResults) {
  std::vector<DNS_RECORDW*> PtrRecords;
  std::vector<DNS_RECORDW*> SrvRecords;
  std::vector<DNS_RECORDW*> TxtRecords;
  std::vector<DNS_RECORDW*> ARecords;

  {
    DNS_RECORDW* current = pQueryResults->pQueryRecords;
    while (current != nullptr) {
      switch (current->wType) {
        case DNS_TYPE_PTR:
          PtrRecords.push_back(current);
          break;
        case DNS_TYPE_SRV:
          SrvRecords.push_back(current);
          break;
        case DNS_TYPE_TEXT:
          TxtRecords.push_back(current);
          break;
        case DNS_TYPE_A:
          ARecords.push_back(current);
          break;
      }
      current = current->pNext;
    }
  }

  for (DNS_RECORDW* Ptr : PtrRecords) {
    if (std::wstring_view{Ptr->pName} != L"_ni-rt._tcp.local") {
      continue;
    }

    std::wstring_view nameHost = Ptr->Data.Ptr.pNameHost;
    DNS_RECORDW* foundSrv = nullptr;
    std::wstring_view foundMac = {};
    for (DNS_RECORDW* Srv : SrvRecords) {
      if (std::wstring_view{Srv->pName} == nameHost) {
        foundSrv = Srv;
        break;
      }
    }
    for (DNS_RECORDW* Txt : TxtRecords) {
      if (std::wstring_view{Txt->pName} == nameHost) {
        for (DWORD i = 0; i < Txt->Data.TXT.dwStringCount; i++) {
          std::wstring_view macAddress = Txt->Data.TXT.pStringArray[i];
          if (macAddress.starts_with(L"MAC=")) {
            foundMac = macAddress;
            break;
          }
        }
        if (!foundMac.empty()) {
          break;
        }
      }
    }
    if (!foundSrv || foundMac.empty()) {
      continue;
    }
    for (DNS_RECORDW* A : ARecords) {
      if (std::wstring_view{A->pName} ==
          std::wstring_view{foundSrv->Data.Srv.pNameTarget}) {
        wpi::SmallString<128> mac;
        wpi::span<const wpi::UTF16> wideMac{
            reinterpret_cast<const wpi::UTF16*>(foundMac.data() + 4),
            foundMac.size() - 4};
        wpi::convertUTF16ToUTF8String(wideMac, mac);

        wpi::SmallString<128> name;
        wpi::span<const wpi::UTF16> wideName{
            reinterpret_cast<const wpi::UTF16*>(A->pName), wcslen(A->pName)};
        wpi::convertUTF16ToUTF8String(wideName, name);

        std::string macStr{mac.str()};
        static_cast<DnsFinder*>(pQueryContext)
            ->OnFound(macStr, A->Data.A.IpAddress, name);
        break;
      }
    }
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
  DNS_STATUS status = DnsServiceBrowse(&request, &pImpl->ServiceCancel);
  return status == DNS_REQUEST_PENDING;
}

void DnsFinder::StopSearch() {
  DnsServiceBrowseCancel(&pImpl->ServiceCancel);
}
#endif
