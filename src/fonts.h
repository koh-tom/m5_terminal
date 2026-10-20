#pragma once
#include <M5GFX.h>

// fonts/*.vlw (tools/fonts.sh で生成) をファームに埋め込んで使う
enum class Font : uint8_t {
  Clock,  // Roboto Light 124 数字のみ
  Num84,
  Num40,
  Lat12,  // Roboto Medium 12 英数字
  Jp13,
  Jp16,   // 第1水準まで入ってる
  Jp20,
  Jp36,   // 在室表示用
  Eva24,  // 明朝 Black
  Count
};

void fontsBegin();
const lgfx::IFont* font(Font f);

// 文字列の全部の文字がフォントにあるか
bool fontHas(Font f, const char* utf8);
