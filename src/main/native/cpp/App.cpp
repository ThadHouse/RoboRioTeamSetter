// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <memory>
#include <string_view>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include <fmt/format.h>
#include <glass/Context.h>
#include <glass/Window.h>
#include <glass/WindowManager.h>
#include <glass/other/Log.h>
#include <imgui.h>
#include <libssh/libssh.h>
#include <wpi/Logger.h>
#include <wpi/fs.h>
#include <wpigui.h>
#include "DnsFinder.h"
#include <unordered_map>
#include <mutex>
#include "wpi/SmallString.h"
#include "DeploySession.h"

namespace gui = wpi::gui;

const char* GetWPILibVersion();

#define GLFWAPI extern "C"
GLFWAPI void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);

int teamNumber;
std::unordered_map<std::string, std::pair<unsigned int, std::string>> foundDevices;
std::mutex devicesLock;

static void DisplayGui() {
  ImGui::GetStyle().WindowRounding = 0;

  // fill entire OS window with this window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  int width, height;
  glfwGetWindowSize(gui::GetSystemWindow(), &width, &height);
  ImGui::SetNextWindowSize(ImVec2(width, height));

  ImGui::Begin("Entries", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  ImGui::BeginMenuBar();
  gui::EmitViewMenu();

  bool about = false;
  if (ImGui::BeginMenu("Info")) {
    if (ImGui::MenuItem("About")) {
      about = true;
    }
    ImGui::EndMenu();
  }
  ImGui::EndMenuBar();

  if (about) {
    ImGui::OpenPopup("About");
    about = false;
  }
  if (ImGui::BeginPopupModal("About")) {
    ImGui::Text("roboRIO Team Number Setter");
    ImGui::Separator();
    ImGui::Text("v%s", GetWPILibVersion());
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::InputInt("Team Number", &teamNumber);

  if (teamNumber < 0) {
    teamNumber = 0;
  }

  ImGui::Columns(4, "Devices");
  ImGui::Text("Name");
  ImGui::NextColumn();
  ImGui::Text("MAC Address");
  ImGui::NextColumn();
  ImGui::Text("IP Address");
  ImGui::NextColumn();
  ImGui::Text("Set");
  ImGui::NextColumn();
  ImGui::Separator();

  std::string setString = fmt::format("Set team to {}", teamNumber);

  {
    std::scoped_lock lock{devicesLock};
    for (auto&& i : foundDevices) {
      ImGui::Text("%s", i.second.second.c_str());
      ImGui::NextColumn();
      ImGui::Text("%s", i.first.c_str());
      ImGui::NextColumn();
      struct in_addr in;
      in.s_addr = i.second.first;
      ImGui::Text("%s", inet_ntoa(in));
      ImGui::NextColumn();
      if (ImGui::Button(setString.c_str())) {
        // TODO
        printf("Hello\n");
        fflush(stdout);
      }
      ImGui::NextColumn();
    }
  }
  ImGui::Columns();
  ImGui::End();
}

void OnDnsFound(const std::string& macAddress, unsigned int ipAddress, std::string_view name) {
  std::scoped_lock lock{devicesLock};
  foundDevices[macAddress] = std::make_pair(ipAddress, std::string(name));
}

void Application() {
  gui::CreateContext();
  glass::CreateContext();

  ssh_init();

  DnsFinder DnsResolver;

  DnsResolver.SetOnFound(OnDnsFound);
  DnsResolver.StartSearch();

  gui::ConfigurePlatformSaveFile("teamnumbersetter.ini");

  gui::AddLateExecute(DisplayGui);
  gui::Initialize("roboRIO Team Number Setter", 600, 400);

  gui::Main();

  DnsResolver.StopSearch();

  glass::DestroyContext();
  gui::DestroyContext();
}
