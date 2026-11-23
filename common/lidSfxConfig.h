#pragma once
// Shared ARM9->ARM7 descriptor. Source data remains live until disable ACK.
struct LidSfxClipConfig {
    const void* samples;
    u32 timer, words, control, frames;
};
struct LidSfxConfig {
    LidSfxClipConfig sleep;
    LidSfxClipConfig wake;
};
// Ordinary SFX ACKs are 24-bit tokens. Bit 24 identifies lid-config ACKs.
// Keep the reply within the range already needed for shared-RAM pointers.
constexpr u32 kLidSfxAckFlag = 0x01000000u;
constexpr u32 kLidSfxTokenMask = 0x00FFFFFFu;
constexpr u32 kLidSfxChannel = 6;
constexpr u32 kLidSfxMaxSampleFrames = 24; // 0.4s at 60 Hz, plus one settling frame.
