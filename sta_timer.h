/**
 * @file sta_timer.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief タイマー関連
 * pthreadを使ってタイマーを実現する
 */

#ifndef _STA_TIMER_H
#define _STA_TIMER_H

#define MAX_NUM_TIMER 4 ///< タイマーの数

/**
 * @brief タイマーイベントの構造体
 * タイマーイベントの構造体
 */
typedef struct _time_event_t {
    int timer_id; ///< タイマーID
    int duration; ///< 持続期間
    void (*func)(void); ///< 起動する関数へのポインタ
    int status; ///< タイマーの状態。0でオフ、1でオン。
    pthread_mutex_t timer_mutex; ///< mutex
    pthread_cond_t timer_cond; ///< 持続期間が切れるか手動でオフされたときにイベントを検知する状態変数。
} time_event_t;

time_event_t sta_timers[MAX_NUM_TIMER]; ///< タイマーの配列

void timer_on(int timer_id, void (*func)(void), int duration);
void timer_off(int timer_id);

#endif
