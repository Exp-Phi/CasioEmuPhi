﻿#include "MemBreakPoint.hpp"
#include "Chipset/CPU.hpp"
#include "Chipset/Chipset.hpp"
#include "Emulator.hpp"
#include "Gui/Hooks.h"
#include "Ui.hpp"
#include "imgui/imgui.h"
#include <cstdint>
#include <cstdlib>
#include <stdlib.h>

MemBreakPoint* membp_cv = 0;

void MemBreakPoint::DrawContent() {
	ImGuiListClipper c;
	static int selected = -1;
	c.Begin(break_point_hash.size());
	char buf[5] = {0};
	while (c.Step()) {

		for (int i = c.DisplayStart; i < c.DisplayEnd; i++) {
			MemBPData_t& data = break_point_hash[i];
			_ltoa(data.addr, buf, 16);
			ImGui::PushID(i);
			if (ImGui::Selectable(buf, selected == i)) {
				selected = i;
			}
			ImGui::PopID();
			if (ImGui::BeginPopupContextItem()) {
				selected = i;
				ImGui::Text(
#if LANGUAGE == 2
					"请选择断点模式:"
#else
					"Choose breakpoint type:"
#endif
				);
				if (ImGui::Button(
#if LANGUAGE == 2
						"查找是什么访问了这个地址"
#else
						"Find who accessed this address"
#endif
						)) {
					target_addr = i;
					data.enableWrite = 0;
					data.records.clear();
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::Button(
#if LANGUAGE == 2
						"查找是什么写入了这个地址"
#else
						"Find who writed this address"
#endif
						)) {
					data.enableWrite = true;
					target_addr = i;
					data.records.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::Separator();
				if (ImGui::Button(
#if LANGUAGE == 2
						"删除此地址"
#else
						"Delete"
#endif
						)) {
					data.records.clear();
					if (target_addr == i) {
						target_addr = -1;
					}
					break_point_hash.erase(break_point_hash.begin() + i);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
	}
}

void MemBreakPoint::DrawFindContent() {
	if (target_addr == -1) {
		ImGui::TextColored(ImVec4(255, 255, 0, 255),
#if LANGUAGE == 2
			"未设置任何断点，请添加地址->右键地址->选择模式"
#else
			"No breakpoints.Please add a breakpoint."
#endif
		);
		return;
	}
	int write = break_point_hash[target_addr].enableWrite;
	static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
	ImGui::Text(
#if LANGUAGE == 2
		"正在监听地址:%04x"
#else
		"Monitoring:%04x"
#endif
		,
		break_point_hash[target_addr].addr);
	ImGui::SameLine();
	static const char* fx = "";
	if (ImGui::Button(
#if LANGUAGE == 2
			"清除记录"
#else
			"Clear records"
#endif
			)) {
		break_point_hash[target_addr].records.clear();
	}
	if (ImGui::BeginTable("##outputtable", 2, flags)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("PC: ");
		ImGui::TableSetupColumn(
#if LANGUAGE == 2
			""
#else
			""
#endif
		);
		ImGui::TableHeadersRow();
		int i = 0;
		for (auto kv : break_point_hash[target_addr].records) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextColored(ImVec4(0, 200, 0, 200), "%01x:%04x", kv.first >> 16, kv.first & 0x0ffff);
			ImGui::TableSetColumnIndex(1);
			ImGui::PushID(i++);
			if (ImGui::Button(
#if LANGUAGE == 2
					"查看调用堆栈"
#else
					"View callstack"
#endif
					)) {
				fx = kv.second.stacktrace.c_str();
				SDL_ShowSimpleMessageBox(0, "", fx, 0);
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

void MemBreakPoint::SetupHooks() {
	SetupHook(on_memory_read, [&](casioemu::MMU& sender, MemoryEventArgs& mea) {
		if (break_on_cv) {
			if (target_addr == -1) {
				return;
			}
			MemBPData_t& bp = break_point_hash.at(target_addr);
			if (bp.addr == mea.offset && !bp.enableWrite) {
				SetDebugbreak();
			}
		}
		else {
			TryTrigBp(mea.offset, 0);
		}
	});
	SetupHook(on_memory_write, [&](casioemu::MMU& sender, MemoryEventArgs& mea) {
		if (break_on_cv) {
			if (target_addr == -1) {
				return;
			}
			MemBPData_t& bp = break_point_hash.at(target_addr);
			if (bp.addr == mea.offset && bp.enableWrite) {
				SetDebugbreak();
			}
		}
		else {
			TryTrigBp(mea.offset, 1);
		}
	});
	membp_cv = this;
}

void MemBreakPoint::TryTrigBp(uint32_t addr, bool write) {
	if (target_addr == -1) {
		return;
	}
	MemBPData_t& bp = break_point_hash.at(target_addr);
	if (bp.addr == addr && bp.enableWrite == write) {
		bp.records[(m_emu->chipset.cpu.reg_csr << 16) | m_emu->chipset.cpu.reg_pc] = Record{m_emu->chipset.cpu.GetBacktrace(), (unsigned int)(m_emu->chipset.cpu.reg_lcsr << 16) | m_emu->chipset.cpu.reg_lr};
	}
}

void MemBreakPoint::RenderCore() {
	static char buf[10] = {0};
	ImGui::BeginChild("##srcollingmbp", ImVec2(0, break_on_cv ? ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() * 6 : ImGui::GetWindowHeight() / 3));
	DrawContent();
	ImGui::EndChild();
	ImGui::SetNextItemWidth(ImGui::CalcTextSize("F").x * 6);
	ImGui::InputText(
		"##addressin",
		buf, 10, ImGuiInputTextFlags_CharsHexadecimal);
	ImGui::SameLine();
	if (ImGui::Button(
#if LANGUAGE == 2
			"添加地址"
#else
			"Add"
#endif
			)) {
		break_point_hash.push_back({.addr = (uint32_t)strtol(buf, nullptr, 16)});
	}
	ImGui::Checkbox(
#if LANGUAGE == 2
		"在Code中显示"
#else
		"Break on op"
#endif
		,
		&break_on_cv);
	if (!break_on_cv) {
		ImGui::BeginChild("##findoutput");
		DrawFindContent();
		ImGui::EndChild();
	}
}

void MemBreakPoint::ExternalAddBp(uint32_t addr, bool write) {
	break_point_hash.push_back({.enableWrite = write, .addr = addr});
	target_addr = break_point_hash.size() - 1;
}

void SetMemBp(uint32_t addr, bool write) {
	if (membp_cv) {
		membp_cv->ExternalAddBp(addr, write);
	}
}
