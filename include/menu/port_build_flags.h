/* Build-time feature switches. Override with compiler -DNAME=0/1. */
#ifndef PORT_BUILD_FLAGS_H
#define PORT_BUILD_FLAGS_H
/* Both peers must allow Sparking! Changes; otherwise online battles use Classic. */
#ifndef ZENKAIPOWER_ONLINE_SPARKING_CHANGES
#define ZENKAIPOWER_ONLINE_SPARKING_CHANGES 1
#endif
#ifndef ZENKAIPOWER_NATIVE_ELEMENTS
#define ZENKAIPOWER_NATIVE_ELEMENTS 0
#endif
#ifndef ZENKAIPOWER_NATIVE_HUD_SHAPES
#define ZENKAIPOWER_NATIVE_HUD_SHAPES 0
#endif
#ifndef ZENKAIPOWER_TEXT_FOLLOW_RES
#define ZENKAIPOWER_TEXT_FOLLOW_RES 1
#endif
#ifndef ZENKAIPOWER_HUD_INFO_FOLLOW_RES
#define ZENKAIPOWER_HUD_INFO_FOLLOW_RES 1
#endif
#endif
