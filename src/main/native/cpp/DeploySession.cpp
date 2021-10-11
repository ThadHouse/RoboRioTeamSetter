#include "DeploySession.h"

#include <memory>
#include <mutex>
#include <string_view>

#include <fmt/core.h>
#include <wpi/SmallString.h>
#include <wpi/StringExtras.h>
#include <wpi/uv/Error.h>
#include <wpi/uv/GetAddrInfo.h>
#include <wpi/uv/Work.h>
#include <wpi/uv/util.h>

#include "SshSession.h"

using namespace sysid;

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

DeploySession::DeploySession(int team, unsigned int ipAddress,
                             wpi::Logger& logger)
    : m_teamNumber{team}, m_ipAddress{ipAddress}, m_logger{logger} {}

void DeploySession::Execute(wpi::uv::Loop& lp) {
  // Lock mutex.
  std::scoped_lock lock{s_mutex};

  auto wreq = std::make_shared<wpi::uv::WorkReq>();

  wreq->afterWork.connect([this] { m_visited.fetch_add(1); });

  wreq->work.connect([this] {
    // Convert to IP address.
    wpi::SmallString<16> ip;
    in_addr addr;
    addr.s_addr = this->m_ipAddress;
    wpi::uv::AddrToName(addr, &ip);
    DEBUG("Trying to establish SSH connection to {}.", ip);
    try {
      if (m_connected) {
        return;
      }
      SshSession session{ip.str(), kPort, kUsername, kPassword, m_logger};
      session.Open();
      DEBUG("SSH connection to {} was successful.", ip);

      // If we were successful, continue with the deploy (after
      // making sure that no other thread also reached this location
      // at the same time).
      if (m_connected.exchange(true)) {
        return;
      }

      SUCCESS("{}", "roboRIO Connected!");

      try {
        session.Execute(fmt::format("/usr/local/natinst/bin/nirtcfg --file=/etc/natinst/share/ni-rt.ini --set section=systemsettings,token=host_name,value=roborio-{}-FRC ; sync", m_teamNumber));
      } catch (const SshSession::SshException& e) {
        ERROR("An exception occurred: {}", e.what());
      }
    } catch (const SshSession::SshException& e) {
      DEBUG("SSH connection to {} failed.", ip);
    }
  });
  wpi::uv::QueueWork(lp, wreq);
}