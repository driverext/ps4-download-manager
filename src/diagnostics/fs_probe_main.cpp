// Stage 2: isolated, read-only filesystem probe.
// A diagnostic ONLY: no download engine, disk writes, HTTP, or Real-Debrid.
// Press SQUARE once to issue sceKernelStat("/data") with a padded ABI buffer.
// X changes the lower bar; neither button invokes the crash-prone exit path.
#include "../platform/graphics.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <sys/stat.h>
#include <sstream>
#include <cstdint>
#include <cstring>

std::stringstream debugLogStream;

struct KernelStatRaw { alignas(16) unsigned char bytes[256]{}; };

int main() {
    Scene2D screen(1920,1080,4);
    if (!screen.Init(0xC000000,2)) return 1;
    OrbisUserServiceInitializeParams params{};
    params.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    if (sceUserServiceInitialize(&params)<0) return 2;
    int user=-1;
    if (sceUserServiceGetInitialUser(&user)<0) return 3;
    if (scePadInit()<0) return 4;
    const int pad=scePadOpen(user,0,0,nullptr);
    if (pad<0) return 5;

    const Color bg{11,18,32}, teal{64,210,208}, white{225,235,250};
    const Color red{225,70,85}, green{75,205,125}, yellow{225,190,80};
    bool crossHeld=false, prevSquare=false, probeDone=false, success=false;
    int result=0;
    for (int frame=0;;++frame) {
        screen.FrameBufferFill(bg);
        screen.DrawRectangle(160,140,1600,120,teal);
        screen.DrawRectangle(160,350,1400,100,white);
        // Bottom bar changes to white when X input is received.
        screen.DrawRectangle(160,520,600,100,crossHeld?white:teal);
        // Fourth bar: yellow=not run; green=success; red=error.
        screen.DrawRectangle(160,710,1200,100,!probeDone?yellow:(success?green:red));
        if (probeDone && !success) {
            // Width encodes low 12 bits of returned result without relying on text.
            screen.DrawRectangle(160,850,static_cast<int>(static_cast<uint32_t>(result)&0xfff),25,red);
        }
        screen.SubmitFlip(frame);
        screen.FrameWait(frame);
        screen.FrameBufferSwap();
        OrbisPadData input{};
        if (scePadReadState(pad,&input)>=0) {
            crossHeld=(input.buttons&ORBIS_PAD_BUTTON_CROSS)!=0;
            const bool square=(input.buttons&ORBIS_PAD_BUTTON_SQUARE)!=0;
            if (square && !prevSquare && !probeDone) {
                KernelStatRaw info;
                // The original build passed OrbisKernelStat as a stack object,
                // which can be too small for the ABI. Use >=256-byte raw storage.
                result=sceKernelStat("/data",reinterpret_cast<OrbisKernelStat*>(info.bytes));
                uint16_t mode=0;
                if (result==0) std::memcpy(&mode,info.bytes+0x08,sizeof(mode));
                success=(result==0 && S_ISDIR(mode));
                probeDone=true;
            }
            prevSquare=square;
        }
    }
}
