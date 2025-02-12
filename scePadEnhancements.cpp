#include "framework.h"
#include "scePadEnhancements.h"
#include <intrin.h>

// reimplemented, inlined on msstore build
int Com_LocalClient_GetControllerIndex(int localClientNum)
{
    ClientGameState* clientGameStates = (ClientGameState*)REBASE(0x8dee250);
    if (localClientNum > 2 || localClientNum < 0)
        return -1;
    if (clientGameStates[localClientNum].localClientNum == localClientNum)
        return clientGameStates[localClientNum].controllerIndex;
    return -1;
}

scePadSetLightBar_t scePadSetLightBar = NULL;
scePadResetLightBar_t scePadResetLightBar = NULL;

#define Scr_GetInt(inst, index) ((uint32_t(*)(uint32_t, uint32_t))REBASE(0x1391240))(inst, index)

int setAllControllersIndex = -1;
MDT_Define_FASTCALL(REBASE(0xC23C20), CScrCmd_SetAllControllersLightbarColor_Hook, void, (uint32_t ctrl))
{
    setAllControllersIndex = 0;
    MDT_ORIGINAL(CScrCmd_SetAllControllersLightbarColor_Hook, (ctrl));
}

int32_t ControllerIndexToScePadHandle(int controllerIndex)
{
    GamePad* gamePadLookup = (GamePad*)REBASE(0x19b7d2b0);
    PadHandleLookup* handleLookup = (PadHandleLookup*)REBASE(0x19b7d260);

    // PC only supports 2 controllers
    if (controllerIndex > 2 || controllerIndex < 0)
        return -1;

    // make sure the controller is connected
    if (!gamePadLookup[controllerIndex].connected) {
        nlog("controller %i not connected", controllerIndex);
        return -1;
    }

    // make sure the controller is a scePad controller and actually connected
    // wtf 3arc this is a really silly way of doing it!
    if (gamePadLookup[controllerIndex].pad_index < 4 || gamePadLookup[controllerIndex].pad_index >= 8) {
        nlog("pad index for %i out of range (%i)", controllerIndex, gamePadLookup[controllerIndex].pad_index);
        return -1;
    }
    int pad_index = gamePadLookup[controllerIndex].pad_index;

    // sanity check
    if (handleLookup[pad_index].status < 0) {
        nlog("pad status for %i=%i invalid (%i)", controllerIndex, pad_index, handleLookup[pad_index].status);
        return -1;
    }
    if (handleLookup[pad_index].handle_id < 0) {
        nlog("scePad handle for %i=%i is %i, wtf?", controllerIndex, pad_index, handleLookup[pad_index].handle_id);
    }

    return handleLookup[pad_index].handle_id;
}

MDT_Define_FASTCALL(REBASE(0xc23930), GPad_SetLightbarColor_Hook, void, (int controllerIndex, scr_vec3_t* colour))
{
    // call the original in case anyone out there liked the razer integration. they're weird as hell for that btw
    MDT_ORIGINAL(GPad_SetLightbarColor_Hook, (controllerIndex, colour));

    // the game optimises out the controllerIndex value as the razer integration doesn't use it
    // so we have to be creative in getting it back
    uint64_t return_addy = (uint64_t)_ReturnAddress() - REBASE(0);
    if (return_addy == 0xC23BF6) { // CScrCmd_SetControllerLightbarColor
        int localClientNum = Scr_GetInt(1, 0);
        controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    }
    else if (return_addy == 0xC23C98) // CScrCmd_SetAllControllersLightbarColor
        controllerIndex = setAllControllersIndex++; // because we can't get the index from the for loop, just act like we're in that loop
    else if (controllerIndex > 2 || controllerIndex < 0) // safety
        controllerIndex = 0;

    // look up the scePad handle for the given controller index
    int32_t scePadHandle = ControllerIndexToScePadHandle(controllerIndex);
    if (scePadHandle < 0)
        return;

    ScePadLightBarState state;
    state.r = colour->x * 255.0;
    state.g = colour->y * 255.0;
    state.b = colour->z * 255.0;
    state.unk = 0;

    // look up the function if we don't have it
    if (scePadSetLightBar == NULL) {
        HMODULE hm = GetModuleHandleA("libScePad.dll");
        if (hm != 0)
            scePadSetLightBar = (scePadSetLightBar_t)GetProcAddress(hm, "scePadSetLightBar");
    }

    // call it
    if (scePadSetLightBar != NULL)
        scePadSetLightBar(scePadHandle, &state);
}

MDT_Define_FASTCALL(REBASE(0xc23af0), GPad_ResetLightbarColor_Hook, void, ())
{
    // call the original in case anyone out there liked the razer integration. they're weird as hell for that btw
    MDT_ORIGINAL(GPad_ResetLightbarColor_Hook, ());

    // it is completely gone from the calling convention here
    int controllerIndex = -1;

    // the game optimises out the controllerIndex value as the razer integration doesn't use it
    // so we have to be creative in getting it back
    uint64_t return_addy = (uint64_t)_ReturnAddress() - REBASE(0);
    if (return_addy == 0xC23C0D) { // CScrCmd_SetControllerLightbarColor
        int localClientNum = Scr_GetInt(1, 0);
        controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    }
    else if (return_addy == 0xC23C9F) // CScrCmd_SetAllControllersLightbarColor
        controllerIndex = setAllControllersIndex++; // because we can't get the index from the for loop, just act like we're in that loop
    else if (controllerIndex > 2 || controllerIndex < 0) // safety
        controllerIndex = 0;

    // look up the scePad handle for the given controller index
    int32_t scePadHandle = ControllerIndexToScePadHandle(controllerIndex);
    if (scePadHandle < 0)
        return;

    // look up the function if we don't have it
    if (scePadResetLightBar == NULL) {
        HMODULE hm = GetModuleHandleA("libScePad.dll");
        if (hm != 0)
            scePadResetLightBar = (scePadResetLightBar_t)GetProcAddress(hm, "scePadResetLightBar");
    }

    // call it
    if (scePadResetLightBar != NULL)
        scePadResetLightBar(scePadHandle);
}

void apply_scePad_enhancements()
{
    MDT_Activate(GPad_SetLightbarColor_Hook);
    MDT_Activate(GPad_ResetLightbarColor_Hook);
    MDT_Activate(CScrCmd_SetAllControllersLightbarColor_Hook);
}
