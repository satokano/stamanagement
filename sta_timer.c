/**
 * @file sta_timer.c
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief タイマー関連
 * pthreadを使ってタイマーを実現する
 */

#include <pthread.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/time.h>
#include "sta_timer.h"

static void *thread_timer_on(void *arg);

/**
 * @brief タイマーを起動する
 *
 * 指定したタイマーを起動する。持続期間、タイマーが切れたときに起動する関数も指定。
 * @param timer_id 起動するタイマーのID
 * @param func 切れたときに起動する関数
 * @param duration 持続期間
 */
void timer_on(int timer_id, void (*func)(void), int duration) {
    pthread_t tid;

    sta_timers[timer_id].timer_id = timer_id;
    sta_timers[timer_id].duration = duration;
    sta_timers[timer_id].func = func;
    sta_timers[timer_id].status = 0;
    pthread_mutex_init(&sta_timers[timer_id].timer_mutex, NULL);
    pthread_cond_init(&sta_timers[timer_id].timer_cond, NULL);

    pthread_create(&tid, NULL, &thread_timer_on, &sta_timers[timer_id]);
}

/**
 * @brief タイマーを停止する
 *
 * 経過時間に関係なく指定したタイマーを停止する
 * @param timer_id 停止するタイマーのID
 */
void timer_off(int timer_id) {
    pthread_mutex_lock(&sta_timers[timer_id].timer_mutex);
    sta_timers[timer_id].status = 0;
    pthread_cond_signal(&sta_timers[timer_id].timer_cond);
    pthread_mutex_unlock(&sta_timers[timer_id].timer_mutex);
}

/**
 * @brief タイマー
 *
 * pthread_cond_timedwaitを使ってtsだけ待つタイマー。切れるとt_event->funcを起動。
 * @param arg time_event_t型の構造体
 * @return NULLを返す
 */
static void *thread_timer_on(void *arg) {
    struct timespec ts;
    struct timeval tv;
    time_event_t *t_event;
    
    t_event = (time_event_t *)arg;

    pthread_detach(pthread_self());

    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec  + t_event->duration;
    ts.tv_nsec = tv.tv_usec * 1000;

    pthread_mutex_lock(&(t_event->timer_mutex));

    t_event->status = 1;
    pthread_cond_timedwait(&(t_event->timer_cond), &(t_event->timer_mutex), &ts);
    if (t_event->status == 1) {
        (*(t_event->func))();
    }

    pthread_mutex_unlock(&(t_event->timer_mutex));

    //free(t_event); // こいつが元凶だった。。。
    return NULL;
}
