/**
 * @file staconfig.c
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA configuration
 *
 * 手動でSTAの設定を行うコマンド
 */

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <math.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>

#include "staconfig.h"

#ifndef _LINUX_IN6_H
/**
 * @brief in6_ifreq
 * 
 * This is in linux/include/net/ipv6.h.
 * 
 * ipv6.hをインクルードするとlinux/in6.hが芋づるされるが
 * in6.hはユーザー空間のアプリケーションでは使用禁止で
 * 代わりにnetinet/in.hを使え、とin6.hに書いてある。
 * だがin6.hだけだとin6_ifreqの定義がないので別に宣言する必要がある
 * ……のだと思う。ifconfigではそうなってるぽい。
 * __u32をuint32_tに書き換えた。__u32はPOSIXだがasm/types.hにしか定義がない。
 */
struct in6_ifreq {
    struct in6_addr ifr6_addr;
    uint32_t ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};
#endif

/**
 * @brief 時空間情報からSTAに変換する
 *
 * 時空間情報からSTAに変換する。
 * @todo 他との一貫性は保つようにすること
 * @todo プレフィックスをちゃんと決める
 * @todo 高さの原点を決めて、下駄をはかせる。-11000mとか。
 * 
 * @param[in] st 時刻と位置情報
 * @param[out] newsta 変換したSTAをいれて返す
 * 
 * @retval 0 成功
 * @retval -1 失敗
 */
static int encode_to_sta(spatio_temporal st, struct in6_addr *newsta) {
	int templatitude;
	int templongitude;
	int tempaltitude;
	int temptime;
	struct tm *tm_temptime;
	char *temp;
	
	if (st.lat > 90.0 || st.lat < -90.0) {
		fprintf(stderr, "[encode_to_sta] latitude range error");
		return -1;
	}
	if (st.lng > 180.0 || st.lng < -180.0) {
		fprintf(stderr, "[encode_to_sta] longitude range error");
		return -1;
	}
	
	// 緯線1m分=4.49*10^(-6)度より、小数点以下6桁までとる
	templatitude = (int)floor((st.lat + 90.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// 緯度 最大180度、小数以下6桁 180000000 --> 28bit このうち上位から26bitを取り出す
	templatitude &= 0xffffffc;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 28bit = %d", templatitude);
	templatitude = (templatitude >> 2);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 26bit = %d", templatitude);


	// 赤道上で経線1m分=8.99*10^(-6)度なので小数点以下5桁でおおよそ十分だが
	// 確実に精度を保証するためと、緯度とそろえるため6桁までとることとした。
	templongitude = (int)floor((st.lng + 180.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// 経度 最大360度、小数点以下6桁 360000000 --> 29bit このうち『上位から』26bitを取り出す
	templongitude &= 0x1ffffff8;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 29bit = %d", templongitude);
	templongitude = (templongitude >> 3);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 26bit = %d", templongitude);
	
	tempaltitude = (int)floor(st.alt / 2.0); // 2m粒度、いまは下駄を足していないし14bitのマスクもかけていない
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] tempaltitude 14bit = %d", tempaltitude);
	
	// timeは10秒粒度とすればもともと14bitなのでマスクする必要なし
	tm_temptime = (struct tm *)malloc(sizeof(struct tm));
	memset(tm_temptime, 0, sizeof(struct tm));
	localtime_r(&(st.time), tm_temptime);
	temptime = (tm_temptime->tm_hour * 60 * 60 + tm_temptime->tm_min * 60 + tm_temptime->tm_sec) / 10;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] temptime 14bit = %d", temptime);
	
	newsta->s6_addr16[7] = htons(((tempaltitude & 0x3) << 14) + temptime); // alt2bit + time14bit
	newsta->s6_addr16[6] = htons(((templatitude & 0xf) << 12) + ((tempaltitude & 0x3ffc) >> 2)); // lat4bit + alt12bit
	newsta->s6_addr16[5] = htons((templatitude & 0xffff0) >> 4); // lat16bit
	newsta->s6_addr16[4] = htons(((templongitude & 0x3ff) << 6) + ((templatitude & 0x3f00000) >> 20)); // lon10bit + lat6bit
	newsta->s6_addr16[3] = htons((templongitude & 0x3fffc00) >> 10); // lon16bit
	newsta->s6_addr16[2] = 0;
	newsta->s6_addr16[1] = htons(0x200);
	newsta->s6_addr16[0] = htons(0x2001);
    
    free(tm_temptime);
	
	temp = (char *)malloc(sizeof(char) * 512);
	if (inet_ntop(AF_INET6, newsta, temp, 512) == NULL) {
		fprintf(stderr, "[encode_to_sta] inet_ntop: %m");
        free(temp);
		return -1;
	}
	
	//fprintf(stderr, "[encode_to_sta] New STA: %s", temp);
    free(temp);
	return 0;
}

/**
 * @brief STAをaddする。
 *
 * インターフェースにSTAをaddする。
 * 
 * @param newsta 新しいSTA
 * @retval 0 成功
 * @retval -1 失敗
 */
static int add_sta(struct sockaddr_in6 *newsta) {
	int fd = -1;
    struct ifreq ifr;
    struct in6_ifreq ifr6;
    char host[NI_MAXHOST];
    
    newsta->sin6_family = AF_INET6;
    newsta->sin6_port = 0;
    memcpy((char *)&ifr6.ifr6_addr, (char *)&(newsta->sin6_addr), sizeof(struct in6_addr));

    fd = get_socket_for_afinet6();
    if (fd < 0) {
        fprintf(stderr, "[add_sta] No support for INET6 on this system.\n");
        return -1;
    }

    strcpy(ifr.ifr_name, parameters.wlan_interface);
    // SIOGIFINDEXとSIOCGIFINDEXは同じ。typo予防のdefine。
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "[add_sta] SIOCGIFINDEX error: %m\n");
        return -1;
    }
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 0;

    // 以下のアドレス設定のioctlはrootでないと実行不可能.
    if (ioctl(fd, SIOCSIFADDR, &ifr6) < 0) {
        fprintf(stderr, "[add_sta] SIOCSIFADDR error: %m\n");
        return -1;
    }
    
    // ログに記録
    getnameinfo((struct sockaddr *)newsta, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    fprintf(stderr, "# add_sta complete, new address = %s", host);
    
    return 0;
}

/**
 * @brief 古いSTAをdelする。
 *
 * インターフェースの古いSTAをdelする。
 * @param oldsta 削除する古いSTA
 * @retval 0 成功
 * @retval -1 失敗
 */
static int delete_sta(struct sockaddr_in6 *oldsta) {
	int fd = -1;
    struct ifreq ifr;
    struct in6_ifreq ifr6;
	
	memcpy((char *)&ifr6.ifr6_addr, (char *)&(oldsta->sin6_addr), sizeof(struct in6_addr));
	
	fd = get_socket_for_afinet6();
	if (fd < 0) {
		fprintf(stderr, "[delete_sta] No support for INET6 on this system.");
        return -1;
	}
	
	strcpy(ifr.ifr_name, parameters.wlan_interface);
	// SIOGIFINDEXとSIOCGIFINDEXは同じ。typo予防のdefine。
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		fprintf(stderr, "[delete_sta] SIOGIFINDEX error: %m\n");
        return -1;
	}
	ifr6.ifr6_ifindex = ifr.ifr_ifindex;
	ifr6.ifr6_prefixlen = 0;
	
	// 以下のアドレス設定のioctlはrootでないと実行不可能.
	if (ioctl(fd, SIOCDIFADDR, &ifr6) < 0) {
		fprintf(stderr, "[delete_sta] SIOCDIFADDR error: %m\n");
		return -1;
	}
	
	return 0;
}

/**
 * @brief STAを取得する
 *
 * 無線LANインターフェースinterfaceに割り当てられたSTAを取得する
 * @param[in] interface 調べる無線LANインターフェース
 * @param[out] sta 取得したSTA
 * @retval 0 成功
 * @retval -1 失敗
 */
static int get_sta(char *interface, struct sockaddr_in6 *sta) {
	int found = 0;
	struct ifaddrs *ifap0, *ifap;
	
    // ath0にSTAが割り当てられているかチェック
    // アドレスがセットされていなければセット
    if (getifaddrs (&ifap0)) {
    	fprintf(stderr, "[get_sta] getifaddrs error: %m\n");
    }
    
    for (ifap = ifap0; ifap; ifap = ifap->ifa_next) {
    	if (strstr(ifap->ifa_name, interface)) {
    		if (ifap->ifa_addr == NULL) {
    			fprintf(stderr, "[get_sta] %s ifa_addr is NULL!!", interface);
    			continue;
    		}
    		if (ifap->ifa_addr->sa_family == AF_INET6) {
    			struct sockaddr_in6 *temp_sockaddr_in6 = (struct sockaddr_in6 *)(ifap->ifa_addr);
    			if (IN6_IS_ADDR_STA(&temp_sockaddr_in6->sin6_addr)) {
    				found = 1;
    				break;
    			} else {
    				continue;
    			}
    		} else {
    			// v4アドレス
    			continue;
    		}
    	} else {
    		// interface以外
    		continue;
    	}
    }
    
    if (!found) {
    	freeifaddrs(ifap0);
    	return -1;
    } else {
        // ifapは関数を抜けると消えるのでコピーしかない？
    	*sta = *((struct sockaddr_in6 *)ifap->ifa_addr);
    }
    freeifaddrs(ifap0);
    return 0;
}


/**
 * @brief STAを表示
 *
 * 無線LANインターフェースinterfaceに割り当てられたSTAを表示
 * @param interface 調べる無線LANインターフェース
 * @retval 0 成功
 * @retval -1 失敗
 */
static int show_sta(char *interface) {
	int ret;
	struct sockaddr_in6 sta;
	char host[NI_MAXHOST];
	
	memset(&sta, 0, sizeof(struct sockaddr_in6));
	memset(host, 0, NI_NUMERICHOST);
	
	if (interface == (char *)NULL) {
	    ret = get_sta(DEFAULT_WLAN_INTERFACE, &sta);
	} else {
		ret = get_sta(interface, &sta);
	}
	
	if (ret == -1) {
		fprintf(stderr, "STA not found.\n");
		return -1;
	} else {
		int err;
    	err = getnameinfo((struct sockaddr *)&sta, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    	if (err != 0) {
    		fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(err));
    		fprintf(stderr, "family=%d\n", sta.sin6_family);
    	}
    	
    	if (interface == (char *)NULL) {
    		printf("%s STA: %s\n", DEFAULT_WLAN_INTERFACE, host);
    	} else {
    		printf("%s STA: %s\n", interface, host);
    	}
	}
	
	return 0;
}

/**
 * @brief AF_INET6用のsocketを作成してfdを返す。
 *
 * AF_INET6用のsocketを作成してそのファイルディスクリプタを返す。
 * net-tools内のlib/af.c、lib/sockets.cなどを参考にした。
 * 
 * @return 作成したファイルディスクリプタ
 */
static int get_socket_for_afinet6() {
	int fd = -1;
    
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    return fd;
}

/**
 * @brief 使用法説明
 *
 * コマンドライン引数の説明を表示して終了する。
 */
static void usage() {
    fprintf(stderr, "Usage: stamd interface latitude longitude altitude [time]\n");
    exit(1);
}

/**
 * @brief グローバル変数の初期化
 *
 * 各種パラメータを保存しているグローバル変数を初期化する。
 */
static void init_parameters(void) {
	memset(&parameters, 0, sizeof(parameters));
	
	strcpy(parameters.wlan_interface, DEFAULT_WLAN_INTERFACE);
}

/**
 * @brief メイン関数
 *
 * メイン関数
 * @return 0 0を返す
 */
int main(int argc, char **argv) {
	int ret;
	char **spp;
	struct ifreq ifr;
	spatio_temporal st;
	struct sockaddr_in6 newsta;
	
    init_parameters();
    
    argv++;
    argc--;
    
    // 単にstaconfigと打たれた
    // 全部表示
    if (argc == 0) {
    	ret = show_sta((char *)NULL);
    	exit(ret < 0);
    }
    
    // staconfig ath0などと指定された
    spp = argv;
    strncpy(ifr.ifr_name, *spp, IFNAMSIZ);
    strncpy(parameters.wlan_interface, *spp, IF_NAMESIZE);
    spp++;
    argc--;
    if (*spp == (char *)NULL) {
    	int err = show_sta(ifr.ifr_name);
    	exit(err < 0);
    }
    
    if (strcmp(*spp, "del") == 0) {
    	struct sockaddr_in6 oldsta;
    	int err;
    	if ((get_sta(parameters.wlan_interface, &oldsta)) == -1) {
    		exit(-1);
    	}
        err = delete_sta(&oldsta);
        exit(err < 0);
    } else if (strcmp(*spp, "add") == 0) {
    	spp++;
    	argc--;
    } else {
    	fprintf(stderr, "Invalid argument.\nChoose add or del.\n");
    	usage();
    	exit(-1);
    }
    
    errno = 0;
    if (argc == 3) {
    	parameters.lat = strtod(*argv++, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "latitude error: %m\n");
    		exit(1);
    	}
    	parameters.lng = strtod(*argv++, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "longitude error: %m\n");
    		exit(1);
    	}
    	parameters.alt = strtod(*argv, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "altitude error: %m\n");
    		exit(1);
    	}
    	parameters.time = (time_t)0;
    } else if (argc == 4) {
    	parameters.lat = strtod(*argv++, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "latitude error: %m\n");
    		exit(1);
    	}
    	parameters.lng = strtod(*argv++, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "longitude error: %m\n");
    		exit(1);
    	}
    	parameters.alt = strtod(*argv++, NULL);
    	if (errno != 0) {
    		fprintf(stderr, "altitude error: %m\n");
    		exit(1);
    	}
    	// time()
    	parameters.time = (time_t)0;
    }
    
    st.lat = parameters.lat;
    st.lng = parameters.lng;
    st.alt = parameters.alt;
    st.time = parameters.time;
    ret = encode_to_sta(st, &newsta.sin6_addr);
    
    if (ret == -1) {
    	fprintf(stderr, "encode_to_sta error.\n");
    	usage();
    	exit(-1);
    }
    ret = add_sta(&newsta);
    
    return 0;
}
