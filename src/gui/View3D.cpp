#include "gui/View3D.h"
#include "chisel/Engine.h"
#include "chisel/Selection.h"
#include "chisel/Chisel.h"
#include "chisel/Handles.h"
#include "input/Input.h"
#include "input/Keyboard.h"
#include "platform/Cursor.h"
#include "common/Time.h"

#include "core/Transform.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/gtc/matrix_inverse.hpp>

#include "gui/IconsMaterialCommunity.h"
#include "gui/Common.h"
#include "render/Render.h"

namespace chisel
{
    inline ConVar<float> cam_maxspeed ("cam_maxspeed",  750.0f,  "Max speed");
    inline ConVar<float> cam_pitchup  ("cam_pitchup",   +89.0f,  "Set the max pitch value.");
    inline ConVar<float> cam_pitchdown("cam_pitchdown", -89.0f,  "Set the min pitch value.");

    inline ConVar<float> m_sensitivity("m_sensitivity", 6.0f,   "Mouse sensitivity");
    inline ConVar<float> m_pitch      ("m_pitch",       0.022f, "Mouse pitch factor.");
    inline ConVar<float> m_yaw        ("m_yaw",         0.022f, "Mouse yaw factor.");

    Camera& View3D::GetCamera() { return camera; }

    void View3D::Start()
    {
        camera.position = vec3(-64.0f, -32.0f, 32.0f) * 32.0f;
        camera.angles = math::radians(vec3(-30.0f, 30.0f, 0));
    }

    uint2 View3D::GetMousePos() const
    {
        ImVec2 absolute = ImGui::GetMousePos();
        return uint2(absolute.x - viewport.x, absolute.y - viewport.y);
    }

    Ray View3D::GetMouseRay()
    {
        return GetCamera().ScreenPointToRay(GetMousePos(), viewport);
    }

// UI //

    bool View3D::BeginMenu(const char* label, const char* name, bool enabled)
    {
        std::string menuName = std::string(label) + "###" + name;
        bool open = ImGui::BeginMenu(menuName.c_str(), enabled);

        if (!open && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name);

        if (open)
        {
            ImGui::TextUnformatted(name);
            ImGui::Separator();
        }

        // Prevent click on viewport when clicking menu
        if (open && ImGui::IsWindowHovered())
            popupOpen = true;
        return open;
    }

    void View3D::NoPadding() {
        // Set window padding to 0
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    }

    void View3D::ResetPadding() {
        ImGui::PopStyleVar();
    }

    void View3D::PreDraw()
    {
        NoPadding();
    }

    void View3D::PostDraw()
    {
        ResetPadding();

        if (!visible)
            return;

        Handles.Begin(viewport, view_axis_allow_flip);

        // Get camera matrices
        Camera& camera = GetCamera();
        mat4x4 view = camera.ViewMatrix();
        mat4x4 proj = camera.ProjMatrix();

        DrawHandles(view, proj);

        // Draw grid
        if (view_grid_show)
            Handles.DrawGrid(camera, view_grid_size);

        OnPostDraw();
    }

    void View3D::Draw()
    {
        popupOpen = false;
        
        ImVec2 startPos = ImGui::GetCursorPos();

        // Return if window is collapsed
        if (!CheckResize()) {
            return;
        }

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
        viewport = Rect(pos.x, pos.y, size.x, size.y);

        // Copy from scene view render target into viewport
        ImGui::GetWindowDrawList()->AddImage(
            GetMainTexture(),
            pos, max,
            ImVec2(0, 0), ImVec2(1, 1)
        );

        // Actually render the viewport
        Render();

        // If mouse is over viewport,
        if (mouseOver = ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && IsMouseOver(viewport))
        {
            Camera& camera = GetCamera();

            // Left-click: Select (or transform selection)
            if (Mouse.GetButtonDown(Mouse::Left) && !popupOpen && !Handles.IsMouseOver())
            {
                OnClick(GetMousePos());
            }

            // Right-click and hold (or press Z) to mouselook
            // TODO: Make Z toggle instead of hold
            if (Mouse.GetButtonDown(Mouse::Right) || (!Keyboard.ctrl && Keyboard.GetKeyDown(Key::Z)))
            {
                Cursor.SetMode(Cursor::Locked);
                Cursor.SetVisible(false);
            }

            if (Mouse.GetButton(Mouse::Right) || (!Keyboard.ctrl && Keyboard.GetKey(Key::Z)))
            {
                MouseLook(Mouse.GetMotion());
            }

            if (Mouse.GetButtonUp(Mouse::Right) || (!Keyboard.ctrl && Keyboard.GetKeyUp(Key::Z)))
            {
                Cursor.SetMode(Cursor::Normal);
                Cursor.SetVisible(true);
            }

            if (!Keyboard.ctrl)
            {
                // WASD. TODO: Virtual axes, arrow keys
                float w = Keyboard.GetKey(Key::W) ? 1.f : 0.f;
                float s = Keyboard.GetKey(Key::S) ? 1.f : 0.f;
                float a = Keyboard.GetKey(Key::A) ? 1.f : 0.f;
                float d = Keyboard.GetKey(Key::D) ? 1.f : 0.f;

                camera.position += camera.Forward() * (w-s) * cam_maxspeed.value * float(Time.deltaTime);
                camera.position += camera.Right() * (d-a) * cam_maxspeed.value * float(Time.deltaTime);
            }
        }

        ResetPadding();
        ImGui::SetCursorPos(startPos);
        if (ImGui::Button(ICON_MC_DOTS_VERTICAL))
            ImGui::OpenPopup("ViewportMenu");

        if (ImGui::BeginPopup("ViewportMenu"))
        {
            OnDrawMenu();

            ImGui::TextUnformatted("Camera");
            ImGui::Separator();

            Camera& camera = GetCamera();
            ImGui::InputFloat("FOV", &camera.fieldOfView);
            ImGui::InputFloat("Speed (hu/s)", &cam_maxspeed.value);
            ImGui::InputFloat("Sensitivity", &m_sensitivity.value);

            ImGui::InputFloat2("Near/Far Z", &camera.near);
            ImGui::SameLine();
            ImGui::Selectable(ICON_MC_HELP_CIRCLE, false, 0, ImGui::CalcTextSize(ICON_MC_HELP_CIRCLE));
            if (ImGui::IsItemHovered())
                GUI::HelpTooltip("Near Z and Far Z control the depth range of the perspective projection.\nSmaller values of Near Z result in a closer near clipping plane, ie. objects get cut off less when close to the camera.\nLarger values of Near Z result in more clipping but much better precision at further distances due to 1/z falloff.\nFar Z controls the far clipping plane, ie. how far objects/brushes will render until.\nSetting Far Z lower may result in better performance on larger maps.\nRecommended values:\nNear Z: 8.0\nFar Z: 16384.0", "Near Z and Far Z");

            ImGui::EndPopup();
        }
        NoPadding();

    }
    
    void View3D::MouseLook(int2 mouse)
    {
        Camera& camera = GetCamera();

        // Pitch down, yaw left
        if (camera.rightHanded)
            mouse = -mouse;

        // Apply mouselook
        vec3 angles = math::degrees(camera.angles);
        angles.xy += vec2(mouse.y * m_pitch, mouse.x * m_yaw) * float(m_sensitivity);

        // Clamp up/down look angles
        angles.x = std::clamp<float>(angles.x, cam_pitchdown, cam_pitchup);

        camera.angles = math::radians(angles);
    }

    // Returns true if window is not collapsed
    bool View3D::CheckResize()
    {
        // If window resized: resize render target
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x != width || size.y != height)
        {
            // Window is collapsed or too small
            if (size.x == 0 || size.y == 0) {
                return false;
            }

            width = uint(size.x);
            height = uint(size.y);

            // Resize framebuffer or viewport
            OnResize(width, height);

            return true;
        }

        return true;
    }
}
