/**
 * @file sta_timer.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief �^�C�}�[�֘A
 * pthread���g���ă^�C�}�[����������
 */

#ifndef _STA_TIMER_H
#define _STA_TIMER_H

#define MAX_NUM_TIMER 4 ///< �^�C�}�[�̐�

/**
 * @brief �^�C�}�[�C�x���g�̍\����
 * �^�C�}�[�C�x���g�̍\����
 */
typedef struct _time_event_t {
    int timer_id; ///< �^�C�}�[ID
    int duration; ///< ��������
    void (*func)(void); ///< �N������֐��ւ̃|�C���^
    int status; ///< �^�C�}�[�̏�ԁB0�ŃI�t�A1�ŃI���B
    pthread_mutex_t timer_mutex; ///< mutex
    pthread_cond_t timer_cond; ///< �������Ԃ��؂�邩�蓮�ŃI�t���ꂽ�Ƃ��ɃC�x���g�����m�����ԕϐ��B
} time_event_t;

time_event_t sta_timers[MAX_NUM_TIMER]; ///< �^�C�}�[�̔z��

void timer_on(int timer_id, void (*func)(void), int duration);
void timer_off(int timer_id);

#endif
