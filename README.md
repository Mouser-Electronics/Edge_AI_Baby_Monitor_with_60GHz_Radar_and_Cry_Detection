# Edge AI Baby Monitor with 60GHz Radar and Cry Detection

Building a Multi-Sensor Monitoring Platform on the Infineon Edge E84 (Mouser).

This repository contains the complete, buildable source for an edge-AI baby-monitoring prototype based on the Infineon KIT_PSE84_AI development board. The system combines a local display interface, live camera preview, 60GHz radar-based motion detection, audio playback, and on-device baby-cry classification. It starts from Infineon's open-source PSOC_Edge_Graphics_LVGL_Demo, version 2.3.0 ([release-v2.3.0](https://github.com/Infineon/mtb-example-psoc-edge-gfx-lvgl-demo/tree/release-v2.3.0); original README: [README_base_demo.md](README_base_demo.md)). Version 2.6.x of the demo uses LVGL v9 and a different folder layout.

> **Note:** This project is an engineering demonstration built on an evaluation platform. It is not a medical device. Its outputs have not been clinically validated and must not be used to determine whether an infant is breathing, present, or safe. The system does not replace caregiver supervision or established safe-sleep practices.

## Hardware

- Infineon KIT_PSE84_AI Edge E84 AI evaluation kit
- 4.3-inch MIPI DSI LCD, 800×480 IPS, capacitive touch
- 0.3MP USB camera module (UVC) on J2
- 4Ω/3W passive speaker on J8 (Molex PicoBlade header, Mouser # 538-53261-0271; housing, Mouser # 538-51021-0200)
- 15-pin 0.5mm pitch flat flex cable (FFC) for the DSI link
- USB Type-C cable for programming, UART debug, and power

## Software

- Infineon ModusToolbox 3.x with GCC Arm toolchain (developed with tools 3.7 and GCC 14.2.1)
- Python 3.x and FFmpeg (only needed to regenerate the audio files)
- Tera Term or similar terminal emulator

## Build and program

The board configuration in this repository already includes the 6MB CM55 flash partition and the SCB0 I²C setup described in the article.

```
make getlibs
make build -j8 CONFIG=Debug TARGET=APP_KIT_PSE84_AI
make program
```

Open a serial terminal on the KitProg3 COM port at 115200 baud.

## Prebuilt firmware

`prebuilt_firmware/app_combined.hex` is the combined image (CM33 secure, CM33 non-secure, CM55) from the Debug build used in this project.

## Repository notes

- All changes are in `proj_cm55`; `proj_cm33_s` and `proj_cm33_ns` are used unmodified.
- `audio-codec-tlv320dac3100/` and `mtb_shared/release-v2.0.0/` (XENSIV BGT60TRxx radar driver) are local driver copies used by the CM55 build.
- **Audio files:** The song and welcome-chime MP3 files used in the article are not included in this repository due to licensing restrictions. To use your own audio, name the files `Children Piano.mp3` (song) and `welcome_sound.mp3` (welcome chime), place them in this folder, install FFmpeg, and run `python regen_audio_48k.py`. The script converts them into `proj_cm55/lullaby_data.c` and `proj_cm55/welcome_sound_data.c`.
- The baby-cry model (`proj_cm55/ready_models`) is the DEEPCRAFT evaluation library from Infineon's [mtb-example-psoc-edge-ml-deepcraft-deploy-ready-model](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-ready-model). Per Infineon, these Ready Models are intended for testing purposes and come with a limited number of inferences (Infineon lists a one-hour runtime for the Baby Cry model).
- `superseded_not_in_build/` holds earlier approaches described in the article. They are **not** part of the build.

## License

Code from Infineon's base demo and libraries remains under Infineon's license terms (see `LICENSE` and each library's own license files).
