// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <memory>
#include <string_view>

#include <fmt/format.h>
#include <glass/Context.h>
#include <glass/Window.h>
#include <glass/WindowManager.h>
#include <glass/other/Log.h>
#include <imgui.h>
#include <libssh/libssh.h>
//#include <uv.h>
#include <wpi/Logger.h>
#include <wpi/fs.h>
#include <wpigui.h>

namespace gui = wpi::gui;

const char* GetWPILibVersion();

#define GLFWAPI extern "C"
GLFWAPI void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);

int teamNumber;

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

    ImGui::BeginMainMenuBar();
    gui::EmitViewMenu();

    bool about = false;
    if (ImGui::BeginMenu("Info")) {
      if (ImGui::MenuItem("About")) {
        about = true;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    ImGui::InputInt("Team Number", &teamNumber);

    if (teamNumber < 0) {
        teamNumber = 0;
    }

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
    ImGui::End();
}

void Application() {
    gui::CreateContext();
    glass::CreateContext();

    ssh_init();

    gui::ConfigurePlatformSaveFile("teamnumbersetter.ini");

    gui::AddLateExecute(DisplayGui);
    gui::Initialize("roboRIO Team Number Setter", 600, 400);

    gui::Main();

    glass::DestroyContext();
    gui::DestroyContext();
}
