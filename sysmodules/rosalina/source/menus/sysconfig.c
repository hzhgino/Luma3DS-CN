/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include "menus/sysconfig.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "ifile.h"

Menu sysconfigMenu = {
    "硬件设置",
    {
        { "WIFI开关", METHOD, .method = &SysConfigMenu_ToggleWireless },
        { "WIFI连接", METHOD, .method = &SysConfigMenu_ControlWifi },
        { "LED开关", METHOD, .method = &SysConfigMenu_ToggleLEDs },
        { "电源键开关", METHOD, .method=&SysConfigMenu_TogglePowerButton },
        { "游戏卡槽开关", METHOD, .method=&SysConfigMenu_ToggleCardIfPower},
        {},
    }
};

bool isConnectionForced = false;

void SysConfigMenu_ToggleLEDs(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "LED开关");
        Draw_DrawString(10, 30, COLOR_WHITE, "按A切换，按B返回。");
        Draw_DrawString(10, 50, COLOR_RED, "警告：");
        Draw_DrawString(10, 70, COLOR_WHITE, "  * 进入休眠模式将重置LED状态！");
        Draw_DrawString(10, 90, COLOR_WHITE, "  * 提示系统电量低的LED灯不能被关闭。");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            mcuHwcInit();
            u8 result;
            MCUHWC_ReadRegister(0x28, &result, 1);
            result = ~result;
            MCUHWC_WriteRegister(0x28, &result, 1);
            mcuHwcExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleWireless(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool nwmRunning = isServiceUsable("nwm::EXT");

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "WIFI开关");
        Draw_DrawString(10, 30, COLOR_WHITE, "按A切换，按B返回。");

        u8 wireless = (*(vu8 *)((0x10140000 | (1u << 31)) + 0x180));

        if(nwmRunning)
        {
            Draw_DrawString(10, 50, COLOR_WHITE, "当前状态：");
            Draw_DrawString(90, 50, (wireless ? COLOR_GREEN : COLOR_RED), (wireless ? " 开启 " : " 关闭"));
        }
        else
        {
            Draw_DrawString(10, 50, COLOR_RED, "NWM 未运行。");
            Draw_DrawString(10, 70, COLOR_RED, "如果你当前在测试菜单，");
            Draw_DrawString(10, 90, COLOR_RED, "退出然后按 R+RIGHT 去切换WiFi。");
            Draw_DrawString(10, 110, COLOR_RED, "否则直接退出并稍等片刻再试。");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A && nwmRunning)
        {
            nwmExtInit();
            NWMEXT_ControlWirelessEnabled(!wireless);
            nwmExtExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_UpdateStatus(bool control)
{
    MenuItem *item = &sysconfigMenu.items[1];

    if(control)
    {
        item->title = "WIFI连接";
        item->method = &SysConfigMenu_ControlWifi;
    }
    else
    {
        item->title = "断开WIFI连接";
        item->method = &SysConfigMenu_DisableForcedWifiConnection;
    }
}

static bool SysConfigMenu_ForceWifiConnection(int slot)
{
    char ssid[0x20 + 1] = {0};
    isConnectionForced = false;

    if(R_FAILED(acInit()))
        return false;

    acuConfig config = {0};
    ACU_CreateDefaultConfig(&config);
    ACU_SetNetworkArea(&config, 2);
    ACU_SetAllowApType(&config, 1 << slot);
    ACU_SetRequestEulaVersion(&config);

    Handle connectEvent = 0;
    svcCreateEvent(&connectEvent, RESET_ONESHOT);

    bool forcedConnection = false;
    if(R_SUCCEEDED(ACU_ConnectAsync(&config, connectEvent)))
    {
        if(R_SUCCEEDED(svcWaitSynchronization(connectEvent, -1)) && R_SUCCEEDED(ACU_GetSSID(ssid)))
            forcedConnection = true;
    }
    svcCloseHandle(connectEvent);

    if(forcedConnection)
    {
        isConnectionForced = true;
        SysConfigMenu_UpdateStatus(false);
    }
    else
        acExit();

    char infoString[80] = {0};
    u32 infoStringColor = forcedConnection ? COLOR_GREEN : COLOR_RED;
    if(forcedConnection)
        sprintf(infoString, "成功强制连接到：%s", ssid);
    else
       sprintf(infoString, "连接到WIFI插槽%d失败", slot + 1);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "WIFI连接");
        Draw_DrawString(10, 30, infoStringColor, infoString);
        Draw_DrawString(10, 70, COLOR_WHITE, "按B返回。");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_B)
            break;
    }
    while(!menuShouldExit);

    return forcedConnection;
}

void SysConfigMenu_TogglePowerButton(void)
{
    u32 mcuIRQMask;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    mcuHwcInit();
    MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
    mcuHwcExit();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "电源键开关");
        Draw_DrawString(10, 30, COLOR_WHITE, "按A切换，按B返回。");

        Draw_DrawString(10, 50, COLOR_WHITE, "当前状态：");
        Draw_DrawString(90, 50, (((mcuIRQMask & 0x00000001) == 0x00000001) ? COLOR_RED : COLOR_GREEN), (((mcuIRQMask & 0x00000001) == 0x00000001) ? " 禁用 " : " 启用 "));

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            mcuHwcInit();
            MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuIRQMask ^= 0x00000001;
            MCUHWC_WriteRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuHwcExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ControlWifi(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    int slot = 0;
    char slotString[12] = {0};
    sprintf(slotString, ">1<  2   3 ");
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "WIFI连接");
        Draw_DrawString(10, 30, COLOR_WHITE, "按A强制连接到插槽:");
        Draw_DrawString(10, 50, COLOR_WHITE, slotString);
        Draw_DrawString(10, 70, COLOR_WHITE, "按B返回。");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if(SysConfigMenu_ForceWifiConnection(slot))
            {
                // Connection successfully forced, return from this menu to prevent ac handle refcount leakage.
                break;
            }

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();
        }
        else if(pressed & KEY_LEFT)
        {
            slotString[slot * 4] = ' ';
            slotString[(slot * 4) + 2] = ' ';
            slot--;
            if(slot == -1)
                slot = 2;
            slotString[slot * 4] = '>';
            slotString[(slot * 4) + 2] = '<';
        }
        else if(pressed & KEY_RIGHT)
        {
            slotString[slot * 4] = ' ';
            slotString[(slot * 4) + 2] = ' ';
            slot++;
            if(slot == 3)
                slot = 0;
            slotString[slot * 4] = '>';
            slotString[(slot * 4) + 2] = '<';
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_DisableForcedWifiConnection(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    acExit();
    SysConfigMenu_UpdateStatus(true);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "断开WIFI连接");
        Draw_DrawString(10, 30, COLOR_WHITE, "断开WIFI连接成功。");

        u32 pressed = waitInputWithTimeout(1000);
        if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleCardIfPower(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool cardIfStatus = false;
    bool updatedCardIfStatus = false;

    do
    {
        Result res = FSUSER_CardSlotGetCardIFPowerStatus(&cardIfStatus);
        if (R_FAILED(res)) cardIfStatus = false;

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "游戏卡槽开关");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "按A切换,按B返回。\n\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "插入或移除游戏卡将重置状态，如果你想运\n行该游戏，则需要重新插入。\n\n");
        Draw_DrawString(10, posY, COLOR_WHITE, "当前状态：");
        Draw_DrawString(90, posY, !cardIfStatus ? COLOR_RED : COLOR_GREEN, !cardIfStatus ? "禁用" : "启用 ");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if (!cardIfStatus)
                res = FSUSER_CardSlotPowerOn(&updatedCardIfStatus);
            else
                res = FSUSER_CardSlotPowerOff(&updatedCardIfStatus);

            if (R_SUCCEEDED(res))
                cardIfStatus = !updatedCardIfStatus;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}
