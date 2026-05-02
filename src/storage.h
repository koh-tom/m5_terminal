#pragma once
#include <stddef.h>

// SDカード (無くても動くようにする)
bool sdBegin();
bool sdReady();

// 1行追記。ファイルが新規ならheaderを先に書く
bool sdAppend(const char* path, const char* line, const char* header = nullptr);
