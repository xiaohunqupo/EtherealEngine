#pragma once

#include "imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "embedded/icons_font_awesome.h"
#include "imgui/imgui_internal.h"
#include "imgui_user/imgui_user.h"
#include "imgui_user/imgui_tabs.h"
#include "imguizmo/imguizmo.h"
#include <memory>
namespace gui
{
using namespace ImGui;

void CleanupTextures();

// Helper function for passing Texture to ImGui::Image.
void Image(std::shared_ptr<void> texture, bool is_rt, bool is_origin_bl, const ImVec2& _size,
		   const ImVec2& _uv0 = ImVec2(0.0f, 0.0f), const ImVec2& _uv1 = ImVec2(1.0f, 1.0f),
		   const ImVec4& _tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
		   const ImVec4& _borderCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

// Helper function for passing Texture to ImGui::ImageButton.
bool ImageButton(std::shared_ptr<void> texture, bool is_rt, bool is_origin_bl, const ImVec2& _size,
				 const ImVec2& _uv0 = ImVec2(0.0f, 0.0f), const ImVec2& _uv1 = ImVec2(1.0f, 1.0f),
				 int _framePadding = -1, const ImVec4& _bgCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
				 const ImVec4& _tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
bool ImageButtonEx(std::shared_ptr<void> texture, const ImVec2& size, const char* tooltip = nullptr,
				   bool selected = false, bool enabled = true);

void ImageWithAspect(std::shared_ptr<void> texture, bool is_rt, bool is_origin_bl, const ImVec2& texture_size, const ImVec2& size,
					 const ImVec2& _uv0 = ImVec2(0, 0), const ImVec2& _uv1 = ImVec2(1, 1),
					 const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
					 const ImVec4& border_col = ImVec4(0, 0, 0, 0));

int ImageButtonWithAspectAndLabel(std::shared_ptr<void> texture, bool is_rt, bool is_origin_bl, const ImVec2& texture_size,
								  const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, bool selected,
								  bool* edit_label, const char* label, char* buf, size_t buf_size,
								  ImGuiInputTextFlags flags = 0);
}
