#pragma once

// Compile-time firmware locale. L10N selects exactly one literal before the
// compiler sees the translation unit, so the other language consumes no flash
// or RAM. PlatformIO's lume-x3-it / lume-x3-en environments define one locale.
#if defined(LUME_LOCALE_IT) && defined(LUME_LOCALE_EN)
#error "Define only one Lume firmware locale"
#elif defined(LUME_LOCALE_EN)
#define LUME_LOCALE_CODE "en"
#define LUME_LOCALE_NAME "English"
#define L10N(english, italian) (english)
#else
// Italian is the product default, including ad-hoc builds without an env flag.
#define LUME_LOCALE_CODE "it"
#define LUME_LOCALE_NAME "Italiano"
#define L10N(english, italian) (italian)
#endif
