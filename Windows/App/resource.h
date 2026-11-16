#pragma once
#define IDI_APP 101
#define IDR_SOUND_VI 201
#define IDR_SOUND_EN 202
#define IDR_EMOJI_JSON 203
#define IDR_REZE_NIGHT 204
#define IDR_REZE_FLOWER 205
#define IDR_REZE_GRAPHIC 206
#define IDR_REZE_ABYSS 207
#ifndef VIKEY_VERSION
#define VIKEY_VERSION "1.0.34"
#endif
#ifndef VIKEY_BUILD
#define VIKEY_BUILD "34"
#endif
#define VIKEY_W2(x) L##x
#define VIKEY_W(x) VIKEY_W2(x)
#define VIKEY_VERSION_W VIKEY_W(VIKEY_VERSION)
