// ============================================================================
// v4.7 interrupt-friendly LZ77 decompression to work RAM
// ----------------------------------------------------------------------------
// The BIOS call LZ77UnCompWram (SWI 0x11) runs with interrupts switched off.
// On a real GBA a few-kilobyte sprite sheet (the player / follower graphics
// that are reloaded while running and turning around, overworld palettes...)
// holds the VBlank interrupt back for milliseconds, and the sound driver, which
// runs from that interrupt, stutters. This C version gives identical output with
// interrupts left on. Only RAM destinations use it: anything aimed at video,
// palette or OAM memory (which can't take 8-bit writes) still goes to the BIOS.
// Every LZ77UnCompWram call in the game is routed here by gba/syscall.h.
// ============================================================================
#include "global.h"

static void BiosLZ77UnCompWram(const void *src, void *dest)
{
    register const void *r0 asm("r0") = src;
    register void *r1 asm("r1") = dest;
    asm volatile("swi 0x11" : "+r"(r0), "+r"(r1) : : "r2", "r3", "memory", "cc");
}

void LZ77UnCompWramIrqFriendly(const void *src, void *dest)
{
    const u8 *s = (const u8 *)src;
    u8 *d = dest;
    u8 *end;

    if ((u32)dest >= 0x05000000 && (u32)dest < 0x08000000)
    {
        BiosLZ77UnCompWram(src, dest);   // palette / VRAM / OAM: 16-bit writes only
        return;
    }
    end = d + (s[1] | (s[2] << 8) | (s[3] << 16));
    s += 4;
    while (d < end)
    {
        u32 flags = *s++, i;

        for (i = 0; i < 8 && d < end; i++, flags <<= 1)
        {
            if (flags & 0x80)
            {
                u32 len = (s[0] >> 4) + 3;
                const u8 *from = d - ((((s[0] & 0xF) << 8) | s[1]) + 1);

                s += 2;
                while (len-- != 0 && d < end)
                    *d++ = *from++;
            }
            else
            {
                *d++ = *s++;
            }
        }
    }
}
