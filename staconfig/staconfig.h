/**
 * @file staconfig.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA configuration
 * 
 * �蓮��STA�̐ݒ���s���R�}���h
 */

#ifndef _STACONFIG_H
#define _STACONFIG_H

#define DEFAULT_WLAN_INTERFACE "ath0"

/**
 * @brief STA�����肷��B
 *
 * �v���t�B�b�N�X���݂�STA���ǂ������肷��B
 * netinet/in.h�ɃA�h���X�̎�ޔ��ʗp�}�N���ȂǕ֗��Ȃ̂����낢�날��̂�
 * ������Q�l�ɂ����B
 */
#define IN6_IS_ADDR_STA(a) \
	(((__const uint16_t *) (a))[0] == htons(0x2001)				      \
	 && ((__const uint16_t *) (a))[1] == htons(0x200)				      \
	 && ((__const uint16_t *) (a))[2] == 0)

/**
 * @brief �����ʒu������
 * 
 * �����ƈʒu�����͂�ێ�����\����
 */
typedef struct _spatio_temporal {
  	time_t time; ///< time
  	double lat; ///< latitude
  	double lng; ///< longitude
  	double alt; ///< altitude
} spatio_temporal;

/**
 * @brief �O���[�o���ȃp�����[�^�Q
 *
 * �O���[�o���ϐ����܂Ƃ߂��\����
 */
typedef struct _global_parameters {
    char wlan_interface[5]; ///< Wireless LAN interface
    double lat; ///< latitude
    double lng; ///< longitude
    double alt; ///< altitude
    time_t time; ///< time
} global_parameters; 

global_parameters parameters;

static int add_sta(struct sockaddr_in6 *newsta);
static int delete_sta(struct sockaddr_in6 *oldsta);
static int get_sta(char *interface, struct sockaddr_in6 *sta);
static int show_sta(char *interface);
static int encode_to_sta(spatio_temporal st, struct in6_addr *newsta);
static int get_socket_for_afinet6();
static void init_parameters(void);

#endif
