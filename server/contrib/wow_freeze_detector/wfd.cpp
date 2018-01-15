/*
 * Copyright (C) 2014 shiro <worldofcorecraft@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "zlib.h"
#include <Windows.h>
#include <cstdint>
#include <d3d9.h>
#include <detours.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

const char* OUTPUT_FILE = "wdf.out";

void hook_endscene();
void hook_packet_process();

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason_for_call, LPVOID)
{
    if (reason_for_call == DLL_PROCESS_ATTACH)
    {
        // Overwrite output file if it already exists
        std::ofstream out(OUTPUT_FILE);
        if (!out.is_open())
            throw std::runtime_error("DllMain: Could not open file");
        out.close();

        // Get WoW's handle
        HANDLE wow_proc = GetCurrentProcess();
        if (wow_proc == 0)
            throw std::runtime_error("GetCurrentProcess failed");

        // Spawn two threads from the main module (we cannot use CreateDevice
        // from DllMain)
        if (CreateRemoteThread(wow_proc, NULL, 0,
                (LPTHREAD_START_ROUTINE)hook_endscene, NULL, 0, NULL) == NULL)
            throw std::runtime_error(
                "CreateRemoteThread for hook_endscene failed");
        if (CreateRemoteThread(wow_proc, NULL, 0,
                (LPTHREAD_START_ROUTINE)hook_packet_process, NULL, 0,
                NULL) == NULL)
            throw std::runtime_error(
                "CreateRemoteThread for hook_packet_process failed");
    }

    return TRUE;
}

typedef HRESULT(WINAPI* endscene_prot)(IDirect3DDevice9*);
endscene_prot real_endscene;
HRESULT WINAPI our_endscene(IDirect3DDevice9* device);

LRESULT CALLBACK msg_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void hook_endscene()
{
    /* We hook EndScene by doing the following:
       1. Create a Window
       2. Create a Direct 3D device
       3. Look at the virtual table and hook the intended EndScene
       4. Cleanup */

    // 1. Create a Window
    WNDCLASS wc = {0};
    wc.lpszClassName = "SomeClassName";
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpfnWndProc = msg_proc;

    if (!RegisterClass(&wc))
        throw std::runtime_error("RegisterClass failed");

    HWND hwnd = CreateWindow("SomeClassName", "", WS_OVERLAPPEDWINDOW, 100, 100,
        100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd)
        throw std::runtime_error("CreateWindow failed");

    // 2. Create a Direct 3D device
    IDirect3D9* d3d;
    IDirect3DDevice9* device = NULL;
    if ((d3d = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        throw std::runtime_error("Direct3DCreate9 failed");

    D3DPRESENT_PARAMETERS params = {0};
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.BackBufferFormat = D3DFMT_UNKNOWN;
    params.Windowed = TRUE;

    auto res = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device);
    if (res != D3D_OK)
        throw std::runtime_error("CreateDevice failed");

    // 3. Hook EndScene
    // The d3d device's structure begins with a hand-made virtual table (check
    // d3d9.h if you'd like to see it).
    // We need to create a device that's the same as the one WoW is using and
    // check the address of the EndScene
    // function their specific device uses.

    DWORD endscene_addr = reinterpret_cast<DWORD*>(
        *reinterpret_cast<DWORD*>(
            device))[42]; // EndScene is at +42 in the V-table
    real_endscene = reinterpret_cast<endscene_prot>(endscene_addr);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(void*&)real_endscene, our_endscene);
    if (DetourTransactionCommit() != NO_ERROR)
        throw std::runtime_error(
            "hook_endscene: DetourTransactionCommit failed");

    // 4. Cleanup
    device->Release();
    d3d->Release();
    DestroyWindow(hwnd);
}

struct recv_data
{
    void* vtable;
    void* buf;
    uint32_t unk1;
    uint32_t unk2;
    uint32_t size;
    uint32_t unk3;
};

// The WoW's receive function is actually a __fastcall, with the thisptr
// passed in ecx and an argument we don't care about in edx
typedef void(__thiscall* recv_parser_t)(void*, void*, recv_data&, void*);
// Pointer to WoW's recv
recv_parser_t real_recv_parser;
// Our detoured recv
void __fastcall our_recv_parser(
    void* this_ptr, uint32_t edx_val, void* unk, recv_data& data, void* unk2);

void hook_packet_process()
{
    // WoW does not use ASLR, so the recv packet handler is always at 0x55F440
    real_recv_parser = (recv_parser_t)0x55F440;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(void*&)real_recv_parser, our_recv_parser);
    if (DetourTransactionCommit() != NO_ERROR)
        throw std::runtime_error(
            "hook_endscene: DetourTransactionCommit failed");
}

DWORD last_tick = GetTickCount();
unsigned int frame = 1;
unsigned int new_world_frame =
    0; // ignore following few frames after a new world packet

std::string pretty_format()
{
    std::stringstream ss;
    ss << "[frame #" << frame << "]: ";
    return ss.str();
}

HRESULT WINAPI our_endscene(IDirect3DDevice9* device)
{
    DWORD curr_tick = GetTickCount();

    if (curr_tick - last_tick > 250 && new_world_frame == 0)
    {
        std::ofstream out(OUTPUT_FILE, std::ios::app);
        if (!out.is_open())
            throw std::runtime_error("our_endscene: unable to open file");

        out << pretty_format() << "POTENTIAL FREEZE (time since last update: "
            << curr_tick - last_tick << " ms)" << std::endl;
    }

    last_tick = curr_tick;
    ++frame;
    if (new_world_frame + 100 < frame)
        new_world_frame = 0;

    return real_endscene(device);
}

DWORD last_packet = GetTickCount();

void __fastcall our_recv_parser(
    void* this_ptr, uint32_t edx_val, void* unk, recv_data& data, void* unk2)
{
    std::ofstream out(OUTPUT_FILE, std::ios::app);
    if (!out.is_open())
        throw std::runtime_error("our_recv_parser: unable to open file");

    // data.buf is in format [2 byte opcode n-2 byte data], size is not part of
    // buf

    DWORD now = GetTickCount();

    // Dump Opcode
    char opcode_buf[4]; // format is "000" (3 digits)
    sprintf(opcode_buf, "%01X%02X", ((unsigned char*)data.buf)[1],
        ((unsigned char*)data.buf)[0]);
    out << pretty_format() << "OPCODE: 0x" << opcode_buf
        << " (time since last packet: " << now - last_packet << " ms)";

    // Deflate opcode 0x1F6 (compressed object update)
    std::vector<unsigned char> res_data;
    res_data.resize(data.size - 2);
    // In case we can't deflate, or not 0x1F6
    memcpy(&res_data[0], (unsigned char*)data.buf + 2, data.size - 2);
    if (strncmp("1F6", opcode_buf, 3) == 0)
    {
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        if (inflateInit(&strm) != Z_OK)
            goto failed_deflate;

        strm.avail_in = data.size - 6;
        strm.next_in = (unsigned char*)data.buf + 6; // First 4 bytes are size
        // 256 KiB, yes this is a shitty design, bite me
        static const uint32_t sz = 1024 * 256;
        res_data.resize(sz);
        strm.avail_out = sz;
        strm.next_out = &res_data[0];
        int res = inflate(&strm, Z_NO_FLUSH);
        inflateEnd(&strm);

        if (res != Z_STREAM_END)
        {
            out << "\nZLIB INFLATE ERR: " << res << " msg: " << strm.msg
                << "\n\n";
            goto serious_zlib_failure;
        }

        res_data.resize(sz - strm.avail_out);

        out << " DEFLATED (size: " << res_data.size() << ")";
    }

failed_deflate:

    out << ":" << std::endl;

    // Dump Data
    for (size_t n = 0, bytes = 0; n < res_data.size(); ++n)
    {
        if (bytes == 0)
            out << "\t ";

        char buf[3];
        sprintf(buf, "%02X ", res_data[n]);
        out << buf;

        if (++bytes == 16 && (n + 1) < res_data.size())
        {
            out << std::endl;
            bytes = 0;
        }
    }

    out << std::endl;

    last_packet = now;

    if (strtol(opcode_buf, NULL, 16) == 0x03E)
        new_world_frame = frame;

serious_zlib_failure:

    __asm
    {
        mov edx, [edx_val]
    } real_recv_parser(this_ptr, unk, data, unk2);
}
