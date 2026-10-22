#pragma once
#include <stdint.h>
#include <time.h>

// 在室状態
enum class Presence : uint8_t { Out, In, Away };

// 誰が変えたか (ログとPCへの通知に載る)
enum class PresenceSource : uint8_t { Button, Focus };

void presenceBegin();
void presenceLoop();

// 時刻が未設定のときと、不在→外出のときは失敗する
bool presenceSet(Presence p, PresenceSource src = PresenceSource::Button);
bool presenceCanSet(Presence p);

// 直前の変更を取り消す (変えてから5秒以内だけ)
bool presenceUndo();
uint32_t presenceUndoLeftMs();  // 取り消せる残り時間 (0なら不可)
Presence presenceUndoTarget();  // 取り消せる変更でどの状態にしたか

Presence presenceGet();
time_t presenceSince();      // 今の状態になった時刻
time_t presenceArrived();    // 今日最初に入室した時刻 (無ければ0)
uint32_t presenceTodaySec();  // 今日の在室時間

// PCとの同期用
uint32_t presenceGen();
uint32_t presenceSeq();  // 最後に記録したイベントの番号
const char* presenceName(Presence p);

// ログから gen/seq より後のイベントを1件ずつ渡す
typedef void (*PresenceEventFn)(const char* csvLine, void* arg);
void presenceEach(uint32_t gen, uint32_t afterSeq, PresenceEventFn fn, void* arg);
