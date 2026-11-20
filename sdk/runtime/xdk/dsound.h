#pragma once
// DirectSound for a title's own code (xdk/dsound.cpp)

namespace dsound {
// a title in lockstep (online): with the clock on, a buffer's GetStatus answers whether it still plays by the title's
// frames (FrameClock, on its game thread), not by the host's mixer, which keeps its own pace for what is heard. Two
// machines' mixers end the same sound a frame apart; a game that frees and reuses its voices on that answer would
// differ by it. Off (the default) GetStatus is the mixer's
void SetFrameClock(bool on);  // on: each buffer's place as the mixer has it now, from then on by FrameClock
void FrameClock(double seconds);  // the frames' playing buffers advance by that much of their rate
}  // namespace dsound
