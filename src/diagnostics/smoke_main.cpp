// Isolated startup, without download manager storage, networking or Real-Debrid.
#include "../platform/graphics.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <sstream>
std::stringstream debugLogStream; // graphics diagnostic dependency

int main() {
    Scene2D screen(1920, 1080, 4);
    if (!screen.Init(0xC000000, 2)) return 1;

    OrbisUserServiceInitializeParams params{};
    params.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    if (sceUserServiceInitialize(&params) < 0) return 2;

    int user=-1;
    if (sceUserServiceGetInitialUser(&user) < 0) return 3;
    if (scePadInit() < 0) return 4;
    int pad=scePadOpen(user, 0, 0, nullptr);
    if (pad < 0) return 5;

    const Color bg{11,18,32}, teal{64,210,208}, white{225,235,250};
    for (int frame=0;frame<3600;++frame) {
        screen.FrameBufferFill(bg);
        screen.DrawRectangle(160,140,1600,120,teal);
        screen.DrawRectangle(160,350,1400,100,white);
        screen.DrawRectangle(160,520,600,100,teal);
        screen.SubmitFlip(frame);
        screen.FrameWait(frame);
        screen.FrameBufferSwap();
        OrbisPadData input{};
        if (scePadReadState(pad,&input)>=0 &&
            (input.buttons & ORBIS_PAD_BUTTON_CROSS)) break;
    }
    scePadClose(pad);
    return 0;
}
