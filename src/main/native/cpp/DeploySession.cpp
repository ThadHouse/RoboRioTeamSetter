#include "DeploySession.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_set>

#include <fmt/core.h>
#include <wpi/SmallString.h>
#include <wpi/StringExtras.h>
#include <wpi/uv/Error.h>
#include <wpi/uv/GetAddrInfo.h>
#include <wpi/uv/Work.h>
#include <wpi/uv/util.h>

#include "SshSession.h"

using namespace sysid;

#ifdef ERROR
#undef ERROR
#endif

// Macros to make logging easier.
#define INFO(fmt, ...) WPI_INFO(m_logger, fmt, __VA_ARGS__)
#define DEBUG(fmt, ...) WPI_DEBUG(m_logger, fmt, __VA_ARGS__)
#define ERROR(fmt, ...) WPI_DEBUG(m_logger, fmt, __VA_ARGS__)
#define SUCCESS(fmt, ...) WPI_LOG(m_logger, kLogSuccess, fmt, __VA_ARGS__)

// roboRIO SSH constants.
static constexpr int kPort = 22;
static constexpr std::string_view kUsername = "admin";
static constexpr std::string_view kPassword = "";

wpi::mutex s_mutex;
std::unordered_set<std::string> s_outstanding;

DeploySession::DeploySession(wpi::Logger& logger) : m_logger{logger} {}

std::future<int> DeploySession::Execute(const std::string& macAddress, int team, unsigned int ipAddress) {
  {
    // Lock mutex.
    std::scoped_lock lock{s_mutex};
    if (!s_outstanding.insert(macAddress).second) {
      throw std::runtime_error{"Already executing"};
    }
  }

  return std::async(std::launch::async, [this]() {
  //  Convert to IP address.
    wpi::SmallString<16> ip;
    in_addr addr;
    addr.s_addr = this->m_ipAddress;
    wpi::uv::AddrToName(addr, &ip);
    DEBUG("Trying to establish SSH connection to {}.", ip);
    try {
      SshSession session{ip.str(), kPort, kUsername, kPassword, m_logger};
      session.Open();
      DEBUG("SSH connection to {} was successful.", ip);

      SUCCESS("{}", "roboRIO Connected!");

      try {
        session.Execute(fmt::format("/usr/local/natinst/bin/nirtcfg --file=/etc/natinst/share/ni-rt.ini --set section=systemsettings,token=host_name,value=roborio-{}-FRC ; sync", m_teamNumber));
      } catch (const SshSession::SshException& e) {
        ERROR("An exception occurred: {}", e.what());
        throw e;
      }
    } catch (const SshSession::SshException& e) {
      DEBUG("SSH connection to {} failed with {}.", ip, e.what());
      throw e;
    }
    return 0;
  });

  // auto wreq = std::make_shared<wpi::uv::WorkReq>();

  // wreq->afterWork.connect([this] { m_visited.fetch_add(1); });

  // wreq->work.connect([this] {
  //
  // });
  // wpi::uv::QueueWork(lp, wreq);
}
