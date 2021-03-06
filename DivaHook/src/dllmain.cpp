#include <iostream>
#include <vector>
#include "windows.h"
#include "MainModule.h"
#include "Components/EmulatorComponent.h"
#include "Components/JvsEmulator.h"
#include "Components/TouchPanelEmulator.h"
#include "Components/SysTimer.h"
#include "Input/Mouse.h"
#include "Input/Keyboard.h"

using namespace DivaHook::Components;

namespace DivaHook
{
	const DWORD JMP_HOOK_SIZE = 0x5;
	const BYTE JMP_OPCODE = 0xE9;
	const BYTE NOP_OPCODE = 0x90;

	const DWORD EngineUpdateHookTargetAddress = 0x0045A76F;
	const DWORD HookByteLength = 0x5;
	const DWORD ReturnAddress = EngineUpdateHookTargetAddress + HookByteLength;

	bool firstUpdateTick = true;
	std::vector<EmulatorComponent*> components;

	void InstallHook(BYTE *address, DWORD overrideAddress, DWORD length)
	{
		DWORD oldProtect, dwBkup, dwRelAddr;

		VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect);

		// calculate the distance between our address and our target location
		// and subtract the 5bytes, which is the size of the jmp
		// (0xE9 0xAA 0xBB 0xCC 0xDD) = 5 bytes
		dwRelAddr = (DWORD)(overrideAddress - (DWORD)address) - JMP_HOOK_SIZE;

		*address = JMP_OPCODE;

		// overwrite the next 4 bytes (which is the size of a DWORD)
		// with the dwRelAddr
		*((DWORD *)(address + 1)) = dwRelAddr;

		// overwrite the remaining bytes with the NOP opcode (0x90)
		for (DWORD x = JMP_HOOK_SIZE; x < length; x++)
			*(address + x) = NOP_OPCODE;

		VirtualProtect(address, length, oldProtect, &dwBkup);
	}

	void InitTick()
	{
		//printf("InitTick(): base init\n");

		components.push_back(new JvsEmulator());
		components.push_back(new TouchPanelEmulator());
		components.push_back(new SysTimer());

		for (auto& component : components)
			component->Initialize();

		MainModule::DivaWindowHandle = FindWindow(0, MainModule::DivaWindowName);
	}

	void UpdateTick()
	{
		if (firstUpdateTick)
		{
			InitTick();
			firstUpdateTick = false;
		}

		//printf("UpdateTick(): base update\n");

		for (auto& component : components)
			component->Update();

		if (MainModule::DivaWindowHandle == NULL || GetForegroundWindow() == MainModule::DivaWindowHandle)
		{
			Input::Keyboard::GetInstance()->PollInput();
			Input::Mouse::GetInstance()->PollInput();

			for (auto& component : components)
				component->UpdateInput();
		}
	}

	void _declspec(naked) FunctionHook()
	{
		_asm
		{
			call UpdateTick
			jmp[ReturnAddress]
		}
	}

	void InitializeHooks()
	{
		InstallHook((BYTE*)EngineUpdateHookTargetAddress, (DWORD)FunctionHook, HookByteLength);
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DivaHook::InitializeHooks();
		DivaHook::MainModule::Module = hModule;
		break;
	}

	return TRUE;
}

