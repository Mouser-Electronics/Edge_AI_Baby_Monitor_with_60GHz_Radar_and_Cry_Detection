# NOT USED IN THE BUILD

**The files in this folder are not compiled or linked into the firmware.**
They are kept for reference only, because the project article describes them as
earlier approaches that were tried and replaced. You do not need them to build,
program, or run the baby monitor.

| File | What it was | Why it was replaced |
| --- | --- | --- |
| `face_detector.c/.h` | Experimental face-recognition integration (Infineon `ifx_face_id` library) | Disabled after its `psram_init()` call corrupted the camera DMA path. The `ifx_face_id` library it needs is not included. |
| `psram_init.c/.h` | External PSRAM (SMIF) bring-up for the face-recognition experiment | Removed once every DMA buffer was moved to SoCMEM. |
| `lullaby_mp3_data.c/.h` | Song stored as an MP3 byte array for on-chip decoding | Replaced by pre-converted 48 kHz PCM (`proj_cm55/lullaby_data.c`), which uses far less CPU. |
| `minimp3.h` | Third-party MP3 decoder, patched (line 1718: scratch struct made `static`) to fix a stack overflow | No longer needed once audio is stored as PCM. |

These files were moved out of `proj_cm55` so that ModusToolbox does not pick them up.
The active firmware does not include or reference any of them.
