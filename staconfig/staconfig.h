/**
 * @file staconfig.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA configuration
 * 
 * 手動でSTAの設定を行うコマンド
 */

#ifndef _STACONFIG_H
#define _STACONFIG_H

#define DEFAULT_WLAN_INTERFACE "ath0"

/**
 * @brief STAか判定する。
 *
 * プレフィックスをみてSTAかどうか判定する。
 * netinet/in.hにアドレスの種類判別用マクロなど便利なのがいろいろあるので
 * それを参考にした。
 */
#define IN6_IS_ADDR_STA(a) \
	(((__const uint16_t *) (a))[0] == htons(0x2001)				      \
	 && ((__const uint16_t *) (a))[1] == htons(0x200)				      \
	 && ((__const uint16_t *) (a))[2] == 0)

/**
 * @brief 時刻位置情報入力
 * 
 * 時刻と位置情報入力を保持する構造体
 */
typedef struct _spatio_temporal {
  	time_t time; ///< time
  	double lat; ///< latitude
  	double lng; ///< longitude
  	double alt; ///< altitude
} spatio_temporal;

/**
 * @brief グローバルなパラメータ群
 *
 * グローバル変数をまとめた構造体
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
