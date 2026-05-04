#pragma once
#include <FS.h>

// ログの保存先。SDがあればSD、無ければ内蔵flash(LittleFS)
bool storageBegin();
bool sdReady();
fs::FS& logFS();

// 1行追記。ファイルが新規ならheaderを先に書く
bool logAppend(const char* path, const char* line, const char* header = nullptr);
