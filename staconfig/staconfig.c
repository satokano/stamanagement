/**
 * @file staconfig.c
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA configuration
 *
 * �蓮��STA�̐ݒ���s���R�}���h
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
 * ipv6.h���C���N���[�h�����linux/in6.h�����Â邳��邪
 * in6.h�̓��[�U�[��Ԃ̃A�v���P�[�V�����ł͎g�p�֎~��
 * �����netinet/in.h���g���A��in6.h�ɏ����Ă���B
 * ����in6.h��������in6_ifreq�̒�`���Ȃ��̂ŕʂɐ錾����K�v������
 * �c�c�̂��Ǝv���Bifconfig�ł͂����Ȃ��Ă�ۂ��B
 * __u32��uint32_t�ɏ����������B__u32��POSIX����asm/types.h�ɂ�����`���Ȃ��B
 */
struct in6_ifreq {
    struct in6_addr ifr6_addr;
    uint32_t ifr6_prefixlen;
    unsigned int ifr6_ifindex;
};
#endif

/**
 * @brief ����ԏ�񂩂�STA�ɕϊ�����
 *
 * ����ԏ�񂩂�STA�ɕϊ�����B
 * @todo ���Ƃ̈�ѐ��͕ۂ悤�ɂ��邱��
 * @todo �v���t�B�b�N�X�������ƌ��߂�
 * @todo �����̌��_�����߂āA���ʂ��͂�����B-11000m�Ƃ��B
 * 
 * @param[in] st �����ƈʒu���
 * @param[out] newsta �ϊ�����STA������ĕԂ�
 * 
 * @retval 0 ����
 * @retval -1 ���s
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
	
	// �ܐ�1m��=4.49*10^(-6)�x���A�����_�ȉ�6���܂łƂ�
	templatitude = (int)floor((st.lat + 90.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// �ܓx �ő�180�x�A�����ȉ�6�� 180000000 --> 28bit ���̂�����ʂ���26bit�����o��
	templatitude &= 0xffffffc;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 28bit = %d", templatitude);
	templatitude = (templatitude >> 2);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 26bit = %d", templatitude);


	// �ԓ���Ōo��1m��=8.99*10^(-6)�x�Ȃ̂ŏ����_�ȉ�5���ł����悻�\������
	// �m���ɐ��x��ۏ؂��邽�߂ƁA�ܓx�Ƃ��낦�邽��6���܂łƂ邱�ƂƂ����B
	templongitude = (int)floor((st.lng + 180.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// �o�x �ő�360�x�A�����_�ȉ�6�� 360000000 --> 29bit ���̂����w��ʂ���x26bit�����o��
	templongitude &= 0x1ffffff8;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 29bit = %d", templongitude);
	templongitude = (templongitude >> 3);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 26bit = %d", templongitude);
	
	tempaltitude = (int)floor(st.alt / 2.0); // 2m���x�A���܂͉��ʂ𑫂��Ă��Ȃ���14bit�̃}�X�N�������Ă��Ȃ�
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] tempaltitude 14bit = %d", tempaltitude);
	
	// time��10�b���x�Ƃ���΂��Ƃ���14bit�Ȃ̂Ń}�X�N����K�v�Ȃ�
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
 * @brief STA��add����B
 *
 * �C���^�[�t�F�[�X��STA��add����B
 * 
 * @param newsta �V����STA
 * @retval 0 ����
 * @retval -1 ���s
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
    // SIOGIFINDEX��SIOCGIFINDEX�͓����Btypo�\�h��define�B
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "[add_sta] SIOCGIFINDEX error: %m\n");
        return -1;
    }
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 0;

    // �ȉ��̃A�h���X�ݒ��ioctl��root�łȂ��Ǝ��s�s�\.
    if (ioctl(fd, SIOCSIFADDR, &ifr6) < 0) {
        fprintf(stderr, "[add_sta] SIOCSIFADDR error: %m\n");
        return -1;
    }
    
    // ���O�ɋL�^
    getnameinfo((struct sockaddr *)newsta, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    fprintf(stderr, "# add_sta complete, new address = %s", host);
    
    return 0;
}

/**
 * @brief �Â�STA��del����B
 *
 * �C���^�[�t�F�[�X�̌Â�STA��del����B
 * @param oldsta �폜����Â�STA
 * @retval 0 ����
 * @retval -1 ���s
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
	// SIOGIFINDEX��SIOCGIFINDEX�͓����Btypo�\�h��define�B
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		fprintf(stderr, "[delete_sta] SIOGIFINDEX error: %m\n");
        return -1;
	}
	ifr6.ifr6_ifindex = ifr.ifr_ifindex;
	ifr6.ifr6_prefixlen = 0;
	
	// �ȉ��̃A�h���X�ݒ��ioctl��root�łȂ��Ǝ��s�s�\.
	if (ioctl(fd, SIOCDIFADDR, &ifr6) < 0) {
		fprintf(stderr, "[delete_sta] SIOCDIFADDR error: %m\n");
		return -1;
	}
	
	return 0;
}

/**
 * @brief STA���擾����
 *
 * ����LAN�C���^�[�t�F�[�Xinterface�Ɋ��蓖�Ă�ꂽSTA���擾����
 * @param[in] interface ���ׂ閳��LAN�C���^�[�t�F�[�X
 * @param[out] sta �擾����STA
 * @retval 0 ����
 * @retval -1 ���s
 */
static int get_sta(char *interface, struct sockaddr_in6 *sta) {
	int found = 0;
	struct ifaddrs *ifap0, *ifap;
	
    // ath0��STA�����蓖�Ă��Ă��邩�`�F�b�N
    // �A�h���X���Z�b�g����Ă��Ȃ���΃Z�b�g
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
    			// v4�A�h���X
    			continue;
    		}
    	} else {
    		// interface�ȊO
    		continue;
    	}
    }
    
    if (!found) {
    	freeifaddrs(ifap0);
    	return -1;
    } else {
        // ifap�͊֐��𔲂���Ə�����̂ŃR�s�[�����Ȃ��H
    	*sta = *((struct sockaddr_in6 *)ifap->ifa_addr);
    }
    freeifaddrs(ifap0);
    return 0;
}


/**
 * @brief STA��\��
 *
 * ����LAN�C���^�[�t�F�[�Xinterface�Ɋ��蓖�Ă�ꂽSTA��\��
 * @param interface ���ׂ閳��LAN�C���^�[�t�F�[�X
 * @retval 0 ����
 * @retval -1 ���s
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
 * @brief AF_INET6�p��socket���쐬����fd��Ԃ��B
 *
 * AF_INET6�p��socket���쐬���Ă��̃t�@�C���f�B�X�N���v�^��Ԃ��B
 * net-tools����lib/af.c�Alib/sockets.c�Ȃǂ��Q�l�ɂ����B
 * 
 * @return �쐬�����t�@�C���f�B�X�N���v�^
 */
static int get_socket_for_afinet6() {
	int fd = -1;
    
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    return fd;
}

/**
 * @brief �g�p�@����
 *
 * �R�}���h���C�������̐�����\�����ďI������B
 */
static void usage() {
    fprintf(stderr, "Usage: stamd interface latitude longitude altitude [time]\n");
    exit(1);
}

/**
 * @brief �O���[�o���ϐ��̏�����
 *
 * �e��p�����[�^��ۑ����Ă���O���[�o���ϐ�������������B
 */
static void init_parameters(void) {
	memset(&parameters, 0, sizeof(parameters));
	
	strcpy(parameters.wlan_interface, DEFAULT_WLAN_INTERFACE);
}

/**
 * @brief ���C���֐�
 *
 * ���C���֐�
 * @return 0 0��Ԃ�
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
    
    // �P��staconfig�Ƒł��ꂽ
    // �S���\��
    if (argc == 0) {
    	ret = show_sta((char *)NULL);
    	exit(ret < 0);
    }
    
    // staconfig ath0�ȂǂƎw�肳�ꂽ
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
