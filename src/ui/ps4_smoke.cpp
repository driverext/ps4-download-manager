// Minimal PS4 video smoke test. No filesystem, network, JSON, or download engine.
// A separate title ID prevents replacing the full application during diagnosis.
#include "../platform/graphics.h"
#include <sstream>
std::stringstream debugLogStream;
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

int main() {
    Scene2D screen(1920, 1080, 4);
    if (!screen.Init(0x2000000, 2)) return 1;
    OrbisUserServiceInitializeParams params{};
    params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    if (sceUserServiceInitialize(&params) < 0) return 2;
    int user = -1;
    if (sceUserServiceGetInitialUser(&user) < 0) return 3;
    if (scePadInit() < 0) return 4;
    const int pad = scePadOpen(user, 0, 0, nullptr);
    if (pad < 0) return 5;
    int frame = 0;
    for (;;) {
        OrbisPadData data{};
        if (scePadReadState(pad, &data) >= 0 &&
            (data.buttons & ORBIS_PAD_BUTTON_CIRCLE)) break;
        screen.FrameBufferFill(Color{11,18,32});
        screen.DrawRectangle(100,100,900,200,Color{64,210,208});
        screen.SubmitFlip(frame);
        screen.FrameWait(frame++);
        screen.FrameBufferSwap();
    }
    scePadClose(pad);
    return 0;
}
