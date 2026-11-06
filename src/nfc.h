#pragma once
#include <Print.h>

// Unit NFC (ST25R3916, Port A)
// 登録したカードをかざすと入室 / 退室。登録はUSBからだけ
bool nfcBegin();
bool nfcReady();
void nfcLoop();

// カードが登録されている (在室はカードでだけ変えられる。ボタンは使えない)
bool nfcLocked();

// "nfc ..." コマンド (probe / add / format / list / del <n> / clear / read)
void nfcCommand(const char* args, Print& out);
