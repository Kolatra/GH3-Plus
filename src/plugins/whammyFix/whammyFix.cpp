#include "GH3Plus.h"
#include "GH3Keys.h"
#include "GH3GlobalAddresses.h"
#include "whammyFix.h"
#include <WinBase.h>

static const LPVOID controllerRelatedStart = (LPVOID)0x00522CD0;
static const LPVOID controllerInputDetour = (LPVOID)0x005249C5;

static uint32_t g_clock[gh3p::GH3_MAX_PLAYERS] = { 0 };
static uint8_t g_prevWhammy[gh3p::GH3_MAX_PLAYERS] = { 0 };
static uint8_t g_prevRealWhammy[gh3p::GH3_MAX_PLAYERS] = { 0 };
static uint64_t g_lastChange[gh3p::GH3_MAX_PLAYERS] = { 0 };

static uint32_t controllerStructBuffer=0xDEADC0DE;

static uint32_t g_structure[gh3p::GH3_MAX_PLAYERS] = { 0 };
static uint32_t g_player[gh3p::GH3_MAX_PLAYERS] = { 0 };

static const uint32_t whammyThreshold = 200;
static const uint8_t minWibble = 24;


uint8_t __stdcall modifyWhammyInput(uint32_t controllerStruct, uint32_t rawInput); 



__declspec(naked) void storeControllerStructNaked()
{
    static const uint32_t returnAddress = 0x00522CD6;
    __asm
    {
        mov controllerStructBuffer, ecx;
        sub esp, 118h
        jmp returnAddress;
    }
}

__declspec(naked) void modifyWhammyInputNaked()
{
    static const uint32_t returnAddress = 0x005249CF;
    __asm
    {
        pushad;

        push esi;
        push eax; //rawInput
        mov eax, controllerStructBuffer;
        mov eax, [eax];
        push eax; //controllerStruct
        call modifyWhammyInput
        pop esi
        mov[esi + 1Dh], al;
        popad;
        
        mov eax, ADDR_byteDebugController;
        cmp [eax], 0;

        jmp returnAddress;
    }
}

uint32_t getPlayerIndex(uint32_t controllerStructPtr)
{

    static uint32_t newestPlayer = 0;

    //use the controllerStruct

    for (uint32_t i = 0; i < gh3p::GH3_MAX_PLAYERS; ++i)
    {
        if (controllerStructPtr == g_structure[i])
            return g_player[i];
    }

    //shift existing cells right and push current at 0

    for (uint32_t i = gh3p::GH3_MAX_PLAYERS - 1; i > 0; --i)
    {
        g_structure[i] = g_structure[i - 1];
        g_player[i] = g_player[i - 1];
    }

    g_structure[0] = controllerStructPtr;
    g_player[0] = newestPlayer;
    newestPlayer = (newestPlayer + 1) % gh3p::GH3_MAX_PLAYERS;

    return g_player[0];
}


uint8_t __stdcall modifyWhammyInput(uint32_t controllerStruct, uint32_t rawInput)
{
    rawInput &= 0xFF;
    uint32_t pIdx = getPlayerIndex(controllerStruct);

    ++g_clock[pIdx];

    uint64_t curTime = GetTickCount64();


    if (g_prevRealWhammy[pIdx] != rawInput)
        g_lastChange[pIdx] = curTime;
    g_prevRealWhammy[pIdx] = rawInput;

    uint8_t whammy = (uint8_t)rawInput;

    if ( (curTime - g_lastChange[pIdx]) < whammyThreshold)
    {
        if (g_clock[pIdx] % 2 == 0)
        {
            if (whammy > 100)
                whammy -= 100;
            else
                whammy = 0;
        }
        else
        {
            if (whammy == 0)
                whammy = minWibble;
        }
    }

    g_prevWhammy[pIdx] = whammy;

    return whammy;
}


void ApplyHack()
{
    for (int i = 0; i < gh3p::GH3_MAX_PLAYERS; ++i)
    {
        g_structure[i] = 0xD15EA5ED;
        g_player[i] = 0xDEADFACE;
    }
    gh3p::WriteJmp(controllerRelatedStart, &storeControllerStructNaked);
    gh3p::WriteJmp(controllerInputDetour, &modifyWhammyInputNaked);
}