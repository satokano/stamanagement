/**
 * @file stamanagement.c
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA Management Daemon
 * STA�̃Z�b�g�A�X�V�Ȃǂ��s���f�[����
 */

/**
 * \mainpage STA Management Daemon
 *
 * \section intro_sec Introduction
 *
 * Middleware������͂��󂯂ăA�h���X���Z�b�g������X�V�����肷��f�[�����v���O�����ł��B
 *
 * \section copying_sec COPYING
 * 
 * Copyright (c) 2006,2007 Satoshi OKANO, Sezaki Lab., University of Tokyo
 *
 * \section install_sec Installation
 *
 * ���̂Ƃ���make�ꔭ��OK�̂͂��ł��B
 *
 * \section license_sec LICENSE
 * MIT License�Ƃ��܂����B
 * �������C���N���[�h���Ă���t�@�C����Q�l�ɂ����t�@�C���̃��C�Z���X��
 * �ǂ��Ȃ��Ă��邩�m�F����K�v����ŁA
 * �ꍇ�ɂ���Ă�GPL�����������\�������邩������܂���B
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <ifaddrs.h>

#if 0
#include <linux/ipv6.h>
#endif

#include <linux/sockios.h>
#include <math.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "stamanagement.h"
#include "sta_timer.h"

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
 * @brief 2��v6�A�h���X�������������ׂ�
 * 
 * 2��v6�A�h���X����������memcmp�Œ��ׂ�Bwinpcap���Q�l�ɂ����B 
 * @param a ��ׂ�A�h���X����1
 * @param b ��ׂ�A�h���X����2
 * @retval 1 2��������
 * @retval 0 �قȂ�
 */
static int in6_addr_equal(const struct in6_addr *a, const struct in6_addr *b) {
	return (memcmp(a, b, sizeof(struct in6_addr)) == 0);
}

/**
 * @brief FIFO����̎�M
 *
 * �~�h���E�F�A����FIFO�o�R�Ńf�[�^����M����
 * @param arg �����g���Ă��Ȃ�
 * @retval 0 0��Ԃ�
 */
void *recv_from_fifo(void *arg) {
	UNUSED(arg);
	
    int fd; ///< FIFO�̂��߂�fd
    int len;
    PositionOut output;
    struct ifaddrs *ifap0, *ifap;
    char host[NI_MAXHOST];
    int found = 0;
    struct sockaddr_in6 sin6;
    struct sockaddr_in6 oldsta_sin6;
    struct in6_addr *oldsta = NULL; ///< oldsta_sin6����in6_addr���w��
    PositionOut decode;
    int duplicate;
    struct timeval tv;

    memset(&output, 0, sizeof(output));

    if ((fd = open(fifo_path, O_RDONLY)) == -1) {
    	syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] %m : open %s", fifo_path);
        return NULL;
    }

    while (!srv_shutdown) {
        len = read(fd, &output, sizeof(output));
        
        // �ȉ��̏������d���Ȃ�悤�Ȃ�ʃX���b�h�ɂ���ׂ�����
        if (len == 0) {
        	fprintf(stderr, "[recv_from_fifo] read size 0\n");
            break;
        } else if (len < 0) {
        	syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] read error: %m");
        	fprintf(stderr, "[recv_from_fifo] read error: %m");
        	continue;
        } else if (len > 0) {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] index=%lu", output.index);

            // ath0��STA�����蓖�Ă��Ă��邩�`�F�b�N
            // �A�h���X���Z�b�g����Ă��Ȃ���΃Z�b�g
            if (getifaddrs(&ifap0)) {
                 syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] getifaddrs error %m");
            }
            for (ifap = ifap0; ifap; ifap = ifap->ifa_next) {
                if (strstr(ifap->ifa_name, wlan_interface)) {
                	if (ifap->ifa_addr == NULL) {
                		syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] %s ifa_addr is NULL!!", wlan_interface);
                		continue;
                	}
                	if (ifap->ifa_addr->sa_family == AF_INET6) {
                		getnameinfo(ifap->ifa_addr, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
                        syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] address = %s", host);
                        
                        struct sockaddr_in6 *temp_sockaddr_in6 = (struct sockaddr_in6 *)(ifap->ifa_addr);
                        if (IN6_IS_ADDR_STA(&temp_sockaddr_in6->sin6_addr)) {
                        	found = 1;
                        	oldsta_sin6 = *temp_sockaddr_in6;
                        	oldsta = &oldsta_sin6.sin6_addr;
                        	break;
                        } else {
                        	continue;
                        }
                	} else {
                		// v4�A�h���X
                		continue;
                	}
                } else {
                	// ath0�ȊO
                    continue;
                }
            }
            
            if (!found) { // ������Ȃ�����
            	encode_to_sta(output, &(sin6.sin6_addr));
            	sin6.sin6_family = AF_INET6;
                
                pthread_mutex_lock(&(temp_address.mutex));
                if (temp_address.flag == DAD) {
                	pthread_mutex_unlock(&(temp_address.mutex));
                	continue;
                }
                
                gettimeofday(&tv, NULL);
                temp_address.generated_time = tv.tv_sec;
                temp_address.address = sin6;
                temp_address.flag = DAD;
                pthread_mutex_unlock(&(temp_address.mutex));
                
                duplicate = allocation_request_start(sin6); // AREQ�𑗂���WT�҂�
            } else { // ��������
            	if (decode_from_sta(oldsta, &decode) == -1) {
            		syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] decode_from_sta error");
            		continue;
            	}
            	
            	if (is_inside_valid_range(&output, &decode)) { // �L���͈͈ȓ��Ȃ甲����
            		// syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] OK, in the STA valid range.");
            		// do nothing.
            	} else { // �͈͂��o�Ă���΁A�A�h���X���X�V
            		//syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_fifo] No! outside the range.");
                    encode_to_sta(output, &(sin6.sin6_addr));
                    sin6.sin6_family = AF_INET6;
                    
                    pthread_mutex_lock(&(temp_address.mutex));
                    if (temp_address.flag == DAD) {
                    	pthread_mutex_unlock(&(temp_address.mutex));
                    	continue;
                    }
                    
                    gettimeofday(&tv, NULL);
                    temp_address.generated_time = tv.tv_sec;
                    temp_address.address = sin6;
                    temp_address.flag = DAD;
                    pthread_mutex_unlock(&(temp_address.mutex));
                    
                    duplicate = allocation_request_start(sin6); // AREQ�𑗂���WT�҂�
            	}
            }
            fprintf(stderr, "srv_shutdown %d\n", srv_shutdown);
        }
        fprintf(stderr, "srv_shutdown %d\n", srv_shutdown);
    }
    fprintf(stderr, "srv_shutdown %d\n", srv_shutdown);
    return NULL;
}

/**
 * @brief �~�h���E�F�A�o�͂���STA�ɕϊ�����
 *
 * �~�h���E�F�A�o�͂���STA�ɕϊ�����B
 * @todo ���ł��g��������������Ȃ����
 * @todo �v���t�B�b�N�X�������ƌ��߂�
 * @todo �����̌��_�����߂āA���ʂ��͂�����B-11000m�Ƃ��B
 * 
 * @param[in] po �~�h���E�F�A����̏o��
 * @param[out] newsta �ϊ�����STA������ĕԂ�
 * 
 * @retval 0 ����
 * @retval -1 ���s
 */
static int encode_to_sta(PositionOut po, struct in6_addr *newsta) {
	int templatitude;
	int templongitude;
	int tempaltitude;
	int temptime;
	struct tm *tm_temptime;
	char *temp;
	
	if (po.lat > 90.0 || po.lat < -90.0) {
		syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] latitude range error");
		return -1;
	}
	if (po.lon > 180.0 || po.lon < -180.0) {
		syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] longitude range error");
		return -1;
	}
	
	// �ܐ�1m��=4.49*10^(-6)�x���A�����_�ȉ�6���܂łƂ�
	templatitude = (int)floor((po.lat + 90.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// �ܓx �ő�180�x�A�����ȉ�6�� 180000000 --> 28bit ���̂�����ʂ���26bit�����o��
	templatitude &= 0xffffffc;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 28bit = %d", templatitude);
	templatitude = (templatitude >> 2);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templatitude 26bit = %d", templatitude);


	// �ԓ���Ōo��1m��=8.99*10^(-6)�x�Ȃ̂ŏ����_�ȉ�5���ł����悻�\������
	// �m���ɐ��x��ۏ؂��邽�߂ƁA�ܓx�Ƃ��낦�邽��6���܂łƂ邱�ƂƂ����B
	templongitude = (int)floor((po.lon + 180.0) * 10.0 * 10.0 * 10.0 * 10.0 * 10.0 * 10.0);
	// �o�x �ő�360�x�A�����_�ȉ�6�� 360000000 --> 29bit ���̂����w��ʂ���x26bit�����o��
	templongitude &= 0x1ffffff8;
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 29bit = %d", templongitude);
	templongitude = (templongitude >> 3);
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] templongitude 26bit = %d", templongitude);
	
	tempaltitude = (int)floor(po.alt / 2.0); // 2m���x�A���܂͉��ʂ𑫂��Ă��Ȃ���14bit�̃}�X�N�������Ă��Ȃ�
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] tempaltitude 14bit = %d", tempaltitude);
	
	// time��10�b���x�Ƃ���΂��Ƃ���14bit�Ȃ̂Ń}�X�N����K�v�Ȃ�
	tm_temptime = (struct tm *)malloc(sizeof(struct tm));
	memset(tm_temptime, 0, sizeof(struct tm));
	localtime_r(&(po.time), tm_temptime);
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
		syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] inet_ntop: %m");
        free(temp);
		return -1;
	}
	
	syslog(LOG_LOCAL0|LOG_DEBUG, "[encode_to_sta] New STA: %s", temp);
    free(temp);
	return 0;
}

/**
 * @brief STA����PositionOut�ɕϊ�����B
 * 
 * STA����PositionOut�ɕϊ�����B
 * @param[in] sta �ϊ���STA
 * @param[out] po �ϊ���PositionOut
 * @retval 0 ����
 * @retval -1 ���s
 */
static int decode_from_sta(struct in6_addr *sta, PositionOut *po) {
	int temp;
	
	if (po == NULL) {
		return -1;
	}
	
	if (!IN6_IS_ADDR_STA(sta)) {
		syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] this is not an sta.");
		return -1;
	}
	
	// �ȉ���STA���ɏ�񂪂Ȃ��̂ŕ����s�\�B
	po->index = 0;
	
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] sta->s6_addr16[0,1,2,3,4,5,6,7] = (%d, %d, %d, %d, %d, %d, %d, %d)", sta->s6_addr16[0], sta->s6_addr16[1], sta->s6_addr16[2], sta->s6_addr16[3], sta->s6_addr16[4], sta->s6_addr16[5], sta->s6_addr16[6], sta->s6_addr16[7]);
	
	po->time = (time_t)(((int)ntohs(sta->s6_addr16[7]) & 0x3fff) * 10); // [7]����14bit
	
	temp = (((int)ntohs(sta->s6_addr16[6]) & 0xfff) << 2) + (((int)ntohs(sta->s6_addr16[7]) & 0xc000) >> 14); // [6]����12bit + [7]���2bit
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] tempaltitude = %d", temp);
	po->alt = temp * 2.0; 
	
	temp = (((int)ntohs(sta->s6_addr16[4]) & 0x3f) << 20) + ((int)ntohs(sta->s6_addr16[5]) << 4) + (((int)ntohs(sta->s6_addr16[6]) & 0xf000) >> 12); // [4]����6bit + [5]16bit�S�� + [6]���4bit
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] templatitude = %d", temp);  
	po->lat = (temp << 2) / 1000000.0 - 90.0;
	
	temp = ((int)ntohs(sta->s6_addr16[3]) << 10) + (((int)ntohs(sta->s6_addr16[4]) & 0xffc0) >> 6); // [3]16bit�S�� + [4]���10bit
	//syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] templongitude = %d", temp);
	po->lon = (temp << 3) / 1000000.0 - 180.0;
	
	syslog(LOG_LOCAL0|LOG_DEBUG, "[decode_from_sta] (time,lng,lat,alt)=(%ld, %f, %f, %f)", po->time, po->lon, po->lat, po->alt);
	
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
        syslog(LOG_LOCAL0|LOG_DEBUG, "[update_sta] No support for INET6 on this system.");
        return -1;
    }

    strcpy(ifr.ifr_name, wlan_interface);
    // SIOGIFINDEX��SIOCGIFINDEX�͓����Btypo�\�h��define�B
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[update_sta] SIOCGIFINDEX error: %m");
        return -1;
    }
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 0;

    // �ȉ��̃A�h���X�ݒ��ioctl��root�łȂ��Ǝ��s�s�\.
    if (ioctl(fd, SIOCSIFADDR, &ifr6) < 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[update_sta] SIOCSIFADDR error: %m");
        return -1;
    }
    
    // ���O�ɋL�^
    getnameinfo((struct sockaddr *)newsta, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    syslog(LOG_LOCAL0|LOG_DEBUG, "# add_sta complete, new address = %s", host);
    
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
		syslog(LOG_LOCAL0|LOG_DEBUG, "[delete_sta] No support for INET6 on this system.");
        return -1;
	}
	
	strcpy(ifr.ifr_name, wlan_interface);
	// SIOGIFINDEX��SIOCGIFINDEX�͓����Btypo�\�h��define�B
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		syslog(LOG_LOCAL0|LOG_DEBUG, "[delete_sta] SIOGIFINDEX error: %m");
        return -1;
	}
	ifr6.ifr6_ifindex = ifr.ifr_ifindex;
	ifr6.ifr6_prefixlen = 0;
	
	// �ȉ��̃A�h���X�ݒ��ioctl��root�łȂ��Ǝ��s�s�\.
	if (ioctl(fd, SIOCDIFADDR, &ifr6) < 0) {
		syslog(LOG_LOCAL0|LOG_DEBUG, "[delete_sta] SIOCDIFADDR error: %m");
		return -1;
	}
	
	return 0;
}

/**
 * @brief AREQ���u���[�h�L���X�g����WT�҂�
 *
 * AREQ(Allocation REQest)�𖳐����a���Ƀu���[�h�L���X�g����WT�b�҂�
 *
 * @retval 0 �d���Ȃ�
 * @retval 1 �d�����Ă���Ƃ̕ԓ�����
 */
static int allocation_request_start(struct sockaddr_in6 newsta) {
    int ret;
    struct sockaddr_in6 toaddr_in6;
    u_int16_t type;
    u_int16_t reserved = 0;
    char *buf;
    char host[NI_MAXHOST];
    int mcast_if;
    socklen_t optlen;
    char *mcast_if_name;
    
    memset(&toaddr_in6, sizeof(toaddr_in6), 0);
    toaddr_in6.sin6_family = AF_INET6;
    toaddr_in6.sin6_addr = in6addr_linklocalmulticast;
    toaddr_in6.sin6_port = htons(udp_port);
    
    type = AREQ;
    
    buf = (char *)malloc(AREQ_PACKET_SIZE * sizeof(char));
    memset(buf, 0, AREQ_PACKET_SIZE);
    memcpy(buf, &type, sizeof(type));
    // memcpy(buf[sizeof(type)], reserved, sizeof(reserved)); // ���̂Ƃ���0�Ȃ̂Ŏ����s�v
    memcpy(&(buf[sizeof(type) + sizeof(reserved)]), &newsta, sizeof(newsta));
    
    // ���O�ɋL�^
    if ((ret = getnameinfo((struct sockaddr *)&newsta, sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST)) != 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_start] getnameinfo error: %s", gai_strerror(ret));
    } else {
        syslog(LOG_LOCAL0|LOG_DEBUG, "# allocation_request_start temp address = %s", host);
    }
    
    // /proc/net/igmp6������Ƃ킩�邪�f�t�H���g��ff02::1�ɂ͎Q�����Ă���͂�
    // Double Check�̂���
    ret = check_allnodes_membership(sockfd, if_nametoindex(WLAN_INTERFACE));
    
    mcast_if = if_nametoindex(WLAN_INTERFACE);
    optlen = sizeof(mcast_if);
    ret = setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &mcast_if, sizeof(mcast_if));
    if (ret != 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_start] setsockopt error: %m");
        return 0;
    }
    
    mcast_if = 0;
    optlen = sizeof(mcast_if);
    if (getsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &mcast_if, &optlen) == 0) {
        mcast_if_name = (char *)malloc(IF_NAMESIZE * sizeof(char));
        memset(mcast_if_name, 0, IF_NAMESIZE * sizeof(char));
        if_indextoname(mcast_if, mcast_if_name);
        syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_start] mcast_if is: %s(%d); optlen=%d", mcast_if_name, mcast_if, optlen);
    } else {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_start] getsockopt failed");
    }
    
    // �u���[�h�L���X�g��send
    ret = sendto(sockfd, buf, AREQ_PACKET_SIZE, 0, (struct sockaddr *)&toaddr_in6, sizeof(toaddr_in6));
    
    // WT�b�̃^�C�}�[�I��
    timer_on(0, &allocation_request_timeout, waiting_time);
    
    free(buf);
    return 0;
}

/**
 * @brief AREQ�u���[�h�L���X�g��̃^�C���A�E�g����
 *
 * AREQ���u���[�h�L���X�g�������ƃ^�C���A�E�g�����Ƃ��̏���
 * �A�h���X���m��A�܂��͏d�����Ă�����P�ɔ�����
 * @return 0 0��Ԃ�
 */
static void allocation_request_timeout() {
    struct ifaddrs *ifap0, *ifap;
    struct sockaddr_in6 mysta_sin6;
    int found = 0;
    
    pthread_mutex_lock(&(temp_address.mutex));
    if (temp_address.flag == DAD) {
        // ������STA�𒲂ׂ�
        if (getifaddrs(&ifap0)) {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_timeout] getifaddrs error %m");
        }
        for (ifap = ifap0; ifap; ifap = ifap->ifa_next) {
            if (strstr(ifap->ifa_name, wlan_interface)) {
                if (ifap->ifa_addr == NULL) {
                    syslog(LOG_LOCAL0|LOG_DEBUG, "[allocation_request_timeout] %s ifa_addr is NULL!!", wlan_interface);
                    continue;
                }
                if (ifap->ifa_addr->sa_family == AF_INET6) {
                    struct sockaddr_in6 *temp_sockaddr_in6 = (struct sockaddr_in6 *)(ifap->ifa_addr);
                    if (IN6_IS_ADDR_STA(&temp_sockaddr_in6->sin6_addr)) {
                    	found = 1;
                        mysta_sin6 = *temp_sockaddr_in6;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    // v4�A�h���X
                    continue;
                }
            } else {
                // ath0�ȊO
                continue;
            }
        }
        
    	if (found == 1) {
            delete_sta(&mysta_sin6);
        }
        add_sta(&(temp_address.address));
        temp_address.flag = NOT_DUPLICATE;
        temp_address.generated_time = 0;
        memset(&(temp_address.address), 0, sizeof(temp_address.address));
    } else if (temp_address.flag == DUPLICATE) {
        // do nothing
        char host[NI_MAXHOST];
        getnameinfo((struct sockaddr *)&(temp_address.address), sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        syslog(LOG_LOCAL0|LOG_DEBUG, "# DUPLICATE [allcation_request_timeout] %s", host);
    }
    pthread_mutex_unlock(&(temp_address.mutex));
}

/**
 * @brief STA�L���͈͓��ɂ��邩�ǂ������肷��B
 *
 * ������STA�L���͈͓��ɂ��邩�ǂ������肷��B
 * @param real ����Ɏg���ŐV�̌��݈ʒu�BLocationmw���FIFO�o�R�Ŏ󂯎�������́B
 * @param decoded ���ݎg�p���Ă���STA����t�Z�����O���b�h�̊�_�̈ʒu�B
 * @retval 1 �͈͓�
 * @retval 0 �͈͊O
 */
static int is_inside_valid_range(const PositionOut * const real, const PositionOut * const decoded) {
    const double cr = 50.0; ///< communication range �������a
    const double lg = 1.0; ///< location granularity �ʒu�̗��x
    int cond1 = 0;
    int cond2 = 0;
    int cond3 = 0;
    int cond4 = 0;
    
    cond1 = (lon2x(real->lon - decoded->lon, real->lat) * lon2x(real->lon - decoded->lon, real->lat) + lat2y(real->lat - decoded->lat) * lat2y(real->lat - decoded->lat) <= (cr * cr));
    cond1 &= (lon2x(real->lon, real->lat) >= lon2x(decoded->lon, decoded->lat) + lg / 2);
    cond1 &= (lat2y(real->lat) >= lat2y(decoded->lat) + lg / 2);
    if (!cond1) return 0;

    cond2 = (lon2x(real->lon - decoded->lon, real->lat) * lon2x(real->lon - decoded->lon, real->lat) + (lat2y(real->lat - decoded->lat) - lg) * (lat2y(real->lat - decoded->lat) - lg) <= (cr * cr));
    cond2 &= (lon2x(real->lon, real->lat) >= lon2x(decoded->lon, decoded->lat) + lg / 2);
    cond2 &= (lat2y(real->lat) <= lat2y(decoded->lat) + lg / 2);
    if (!cond2) return 0;

    cond3 = ((lon2x(real->lon - decoded->lon, real->lat) - lg) * (lon2x(real->lon - decoded->lon, real->lat) - lg) + lat2y(real->lat - decoded->lat) * lat2y(real->lat - decoded->lat) <= (cr * cr));
    cond3 &= (lon2x(real->lon, real->lat) <= lon2x(decoded->lon, decoded->lat) + lg / 2);
    cond3 &= (lat2y(real->lat) >= lat2y(decoded->lat) + lg / 2);
    if (!cond3) return 0;

    cond4 = ((lon2x(real->lon - decoded->lon, real->lat) - lg) * (lon2x(real->lon - decoded->lon, real->lat) - lg) + (lat2y(real->lat - decoded->lat) - lg) * (lat2y(real->lat - decoded->lat) - lg) <= (cr * cr));
    cond4 &= (lon2x(real->lon, real->lat) <= lon2x(decoded->lon, decoded->lat) + lg / 2);
    cond4 &= (lat2y(real->lat) <= lat2y(decoded->lat) + lg / 2);
    if (!cond4) return 0;

	if (cond1 && cond2 && cond3 && cond4) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @brief �ܓx��m�P�ʂɕϊ�����
 * 
 * �ܓx�̍�����y������m�P�ʂ̒����ɕϊ�����B
 * �ܓx1�x=��111km
 * @sa http://ja.wikipedia.org/wiki/%E7%B7%AF%E5%BA%A6
 * @sa http://www.rikanenpyo.jp/member/?module=Member&p=Contents%26page%3D2_MS2keyi%3Ams2i%5EContents%26page%3D2_MS2_458%3Agai%5EHilight%26page%3D2_cGEx11x0020_2007_1%26no%3D458%23keyword%3Aco&action=Hilight&page=2_cGEx11x0020_2007_1&no=458#keyword
 *
 * @param lat �ܓx�����̍���
 * @return m�P�ʂ̒���
 */
inline static double lat2y(double lat) {
	return lat * 110952.0;
}

/**
 * @brief �ܓxlat�ł̌o��lon�x���̒�����m�P�ʂŋ��߂�B
 *
 * �ܓxlat�ł̌o��lon�x���̒�����m�P�ʂŋ��߂�B
 * 
 * @param lon �ϊ����o�������̍���
 * @param lat �v�Z����n�_�̈ܓx
 * @return m�P�ʂ̒���
 */
inline static double lon2x(double lon, double lat) {
	return 111319.0 * lon * cos(lat);
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
 * @brief DAD�̂��߂�UDP�\�P�b�g��������
 *
 * DAD�̌��ʎ�MUDP�\�P�b�g��������
 * @retval -1 ���s
 * @retval 0 ����
 */
static int init_udp_socket(pthread_t recv_from_udp_thread_id) {
    int status;
    int one = 1;
    pthread_attr_t detached_attr;
    struct sockaddr_in6 my_sockaddr_in6;
    
    memset(&my_sockaddr_in6, sizeof(my_sockaddr_in6), 0);
    my_sockaddr_in6.sin6_family = AF_INET6;
    my_sockaddr_in6.sin6_addr = in6addr_any;
    my_sockaddr_in6.sin6_port = htons(udp_port); // ����͂������������B�N���C�A���g���̃|�[�g�������œK���ɐݒ肷��ɂ͂ǂ�����΂����񂾂����c
    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) != 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[init_udp_socket] setsockopt error: %m");
        return -1;
    }
    
    if (bind(sockfd, (struct sockaddr *)&my_sockaddr_in6, sizeof(my_sockaddr_in6)) == -1) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[init_udp_socket] bind error: %m");
        return -1;
    }
    
    pthread_attr_init(&detached_attr);
    pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
    
    status = pthread_create(&recv_from_udp_thread_id, &detached_attr, recv_from_udp, NULL);
    // status = pthread_create(&recv_from_udp_thread_id, NULL, recv_from_udp, NULL);
    if (status != 0) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[init_udp_socket] pthread_create error: %m");
    }
    
    return status;
}

/**
 * @brief ���A�h���X�̍\���̂�������
 *
 * ���A�h���X�̍\���̂�����������B
 * 
 */
void init_temporary_address_status() {
    memset(&(temp_address.address), 0, sizeof(temp_address.address));
    temp_address.generated_time = 0;
    pthread_mutex_init(&(temp_address.mutex), NULL);
    temp_address.flag = NOT_DUPLICATE;
}

/**
 * @brief DAD�̂��߂�UDP��M����
 *
 * DAD�̌��ʂ̎�M�X���b�h�����B
 * ��M�������Ă����ɂ���Ȃ�q�X���b�h�ɏ�����C����B
 * �������X�^�[�^�̏ꍇ��DAD�̕ԓ����󂯎��A���]���o�̏ꍇ��AREQ���󂯎��
 */
void *recv_from_udp(void *arg) {
	UNUSED(arg);
	
    int status;
    int len;
    char *buf;
    
    while (!srv_shutdown) {
        pthread_t recv_from_udp_child_thread_id;
        pthread_attr_t detached_attr;
        struct sockaddr_storage from;
        socklen_t fromlen;
        pthreadarg_addr_buf *pthreadarg;
        
        fromlen = sizeof(from);
        
        buf = (char *)malloc(UDP_RECV_BUF_SIZE * sizeof(char));
        pthreadarg = (pthreadarg_addr_buf *)malloc(sizeof(pthreadarg_addr_buf));
        
        pthread_attr_init(&detached_attr);
        pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
        
        len = recvfrom(sockfd, buf, UDP_RECV_BUF_SIZE, 0, (struct sockaddr *)&from, &fromlen);
        if (len < 0) {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp] recvfrom error: %m");
            continue;
        } else if (len == 0) {
            char theotherside[NI_MAXHOST];
            int ret;
            ret = getnameinfo((struct sockaddr *)&from, sizeof(from), theotherside, sizeof(theotherside), NULL, 0, NI_NUMERICHOST);
            if (ret != 0) {
                syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp] getnameinfo error: %s", gai_strerror(ret));
            } else {
                syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp] the other side(%s) shut down normally.", theotherside);
            }
        } else {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp] %d", len);
        }
        
        memcpy(&(pthreadarg->fromaddr), &from, sizeof(struct sockaddr_storage));
        pthreadarg->buf = buf;
        pthreadarg->usedlen = len;
        
        status = pthread_create(&recv_from_udp_child_thread_id, &detached_attr, recv_from_udp_child, pthreadarg);
        if (status != 0) {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp] pthread_create error: %m");
        }
    }
    return NULL;
}

/**
 * @brief DAD�̂��߂�UDP��M�A�q�X���b�h
 *
 * ���ۂ̏������󂯎��q�X���b�h
 * �������X�^�[�^�̏ꍇ��DAD�̕ԓ����󂯎��A���]���o�̏ꍇ��AREQ���󂯎��
 */
void *recv_from_udp_child(void *arg) {
    pthreadarg_addr_buf *pthreadarg = (pthreadarg_addr_buf *)arg;
    struct sockaddr_in6 *fromaddr = (struct sockaddr_in6 *)&(pthreadarg->fromaddr);
    int ret;
    u_int16_t type;
    char *buf;
    
    if (packet_type_is_areq(pthreadarg->buf)) { // ���]���o�̏ꍇ�AAREQ���󂯂�
        struct sockaddr_in6 requested_address;
        arep_flag_reserved flag_reserved;
        struct ifaddrs *ifap0, *ifap;
        struct sockaddr_in6 mysta_sin6;
        
        memcpy(&requested_address, &(pthreadarg->buf[sizeof(type)]), sizeof(requested_address));
        memset(&flag_reserved, 0, sizeof(flag_reserved));
        
        type = AREP;
        buf = (char *)malloc(AREP_PACKET_SIZE * sizeof(char));
        memset(buf, 0, AREP_PACKET_SIZE);
        memcpy(buf, &type, sizeof(type));
        memcpy(&(buf[sizeof(type) + sizeof(flag_reserved)]), &requested_address, sizeof(requested_address));
        
        // ������STA�𒲂ׂ�
        if (getifaddrs(&ifap0)) {
            syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp_child] getifaddrs error %m");
        }
        for (ifap = ifap0; ifap; ifap = ifap->ifa_next) {
            if (strstr(ifap->ifa_name, wlan_interface)) {
                if (ifap->ifa_addr == NULL) {
                    syslog(LOG_LOCAL0|LOG_DEBUG, "[recv_from_udp_child] %s ifa_addr is NULL!!", wlan_interface);
                    continue;
                }
                if (ifap->ifa_addr->sa_family == AF_INET6) {
                    struct sockaddr_in6 *temp_sockaddr_in6 = (struct sockaddr_in6 *)(ifap->ifa_addr);
                    if (IN6_IS_ADDR_STA(&temp_sockaddr_in6->sin6_addr)) {
                        mysta_sin6 = *temp_sockaddr_in6;
                        break;
                    } else {
                        continue;
                    }
                } else {
                    // v4�A�h���X
                    continue;
                }
            } else {
                // ath0�ȊO
                continue;
            }
        }
        
        if (in6_addr_equal(&(requested_address.sin6_addr), &(mysta_sin6.sin6_addr)) == 0) { // �����̃A�h���X�ƈقȂ�
            // AREP_FLAG=0�̃p�P�b�g��Ԃ�
            flag_reserved.arep_flag = 0;
            memcpy(&(buf[sizeof(type)]), &flag_reserved, sizeof(flag_reserved));
            ret = sendto(sockfd, buf, AREP_PACKET_SIZE, 0, (struct sockaddr *)fromaddr, sizeof(fromaddr));
        } else { // �����A�d��
            // AREP_FLAG=1�̃p�P�b�g��Ԃ�
            flag_reserved.arep_flag = 1;
            memcpy(&(buf[sizeof(type)]), &flag_reserved, sizeof(flag_reserved));
            ret = sendto(sockfd, buf, AREP_PACKET_SIZE, 0, (struct sockaddr *)fromaddr, sizeof(fromaddr));
        }
        
        free(buf);
        return NULL;
        
    } else if (packet_type_is_arep(pthreadarg->buf)) { // �X�^�[�^�̏ꍇ�AAREP(DAD�̕ԓ�)���󂯎��
        if (is_duplicate(pthreadarg->buf) == 0) {
            return NULL; // do nothing
        } else { // �d������
            // �^�C�}�[�������~�߂ăC�x���g����������
            char host[NI_MAXHOST];
            pthread_mutex_lock(&(temp_address.mutex));
            temp_address.flag = DUPLICATE;
            getnameinfo((struct sockaddr *)&(temp_address.address), sizeof(struct sockaddr_in6), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
            pthread_mutex_unlock(&(temp_address.mutex));
            syslog(LOG_LOCAL0|LOG_DEBUG, "# DUPLICATE [recv_from_udp_child] %s", host);
            timer_off(0);
        }
    }
    return NULL;
}

/**
 * @brief AREQ���ǂ������肷��
 *
 * �p�P�b�g��AREQ���ǂ������肷��B
 * @param buf �p�P�b�g�ւ̃|�C���^
 * @retval 0 AREQ�ȊO
 * @retval 1 AREQ
 */
inline static int packet_type_is_areq(char *buf) {
    u_int16_t type;
    memcpy(&type, buf, sizeof(type));
    
    if (type == AREQ) {
        return 1;
    } else { // ����ȊO
        return 0;
    }
}

/**
 * @brief AREP���ǂ������肷��
 *
 * �p�P�b�g��AREP���ǂ������肷��B
 * @parm buf �p�P�b�g�ւ̃|�C���^
 * @retval 0 AREP�ȊO
 * @retval 1 AREP
 */
inline static int packet_type_is_arep(char *buf) {
    u_int16_t type;
    memcpy(&type, buf, sizeof(type));
    
    if (type == AREP) {
        return 1;
    } else { // ����ȊO
        return 0;
    }
}

/**
 * @brief �d�����肩�Ȃ������肷��
 *
 * AREP�p�P�b�g�̃t���O�����ďd�����肩�Ȃ������肷��B
 * @note �ǂ���Ƃ�����ł��Ȃ��ꍇ��abort����B
 * @param buf �p�P�b�g�ւ̃|�C���^
 * @retval 0 �d���Ȃ�
 * @retval 1 �d������
 */
inline static int is_duplicate(char *buf) {
    if (buf[16] == 0) { // �d���Ȃ�
        return 0;
    } else if (buf[16] == 1) { // �d������
        return 1;
    } else {
        assert(0);
    }
}

/**
 * @brief �S�m�[�h�����N���[�J���}���`�L���X�g�ɎQ���o�^
 *
 * �S�m�[�h�����N���[�J���}���`�L���X�g�ɂ��炽�߂Ė����I�ɎQ���o�^����B
 * radvd 1.0�̃\�[�X�R�[�h���Q�l�ɂ����B
 *
 * @param sock �`�F�b�N����\�P�b�g
 * @param if_index �`�F�b�N����C���^�[�t�F�[�X�̃C���f�b�N�X�ԍ�
 * @retval 0 ����
 * @retval -1 ���s
 */
static int setup_allnodes_membership(int sock, unsigned int if_index) {
    struct ipv6_mreq mreq;
    char *if_name;
    
    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_interface = if_index;
    
    /* ipv6-allnodes: ff02::1 */
    mreq.ipv6mr_multiaddr.s6_addr32[0] = htonl(0xFF020000);
    mreq.ipv6mr_multiaddr.s6_addr32[3] = htonl(0x1);

    if (setsockopt(sock, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        /* linux-2.6.12-bk4 returns error with HUP signal but keep listening */
        if (errno != EADDRINUSE) {
            if_name = (char *)malloc(IF_NAMESIZE * sizeof(char));
            memset(if_name, 0, IF_NAMESIZE * sizeof(char));
            if_indextoname(if_index, if_name);
            syslog(LOG_LOCAL0|LOG_DEBUG, "[setup_allnodes_membership] can't join ipv6-allnodes on %s(%d)", if_name, if_index);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief �S�m�[�h�����N���[�J���}���`�L���X�g�ւ̎Q���o�^�`�F�b�N
 *
 * �S�m�[�h�����N���[�J���}���`�L���X�g�ɎQ�����Ă��邩�`�F�b�N���A�Q�����Ă��Ȃ���΂��炽�߂ĎQ���o�^����B
 * radvd 1.0�̃\�[�X�R�[�h���Q�l�ɂ����B
 *
 * @param sock �`�F�b�N����\�P�b�g�A�����I�ɂ�setup_allnodes_membership�ɓn�����߂����Ɏg��
 * @param if_index �`�F�b�N����C���^�[�t�F�[�X�̃C���f�b�N�X�ԍ�
 * @retval 0 ����
 * @retval -1 ���s
 */
static int check_allnodes_membership(int sock, unsigned int if_index) {
    FILE *fp;
    unsigned int idx, allnodes_ok = 0;
    char addr[32 + 1];
    int ret = 0;
    char *if_name;

    if ((fp = fopen(PATH_PROC_NET_IGMP6, "r")) == NULL) {
        syslog(LOG_LOCAL0|LOG_DEBUG, "[check_allnodes_membership] can't open %s: %m", PATH_PROC_NET_IGMP6);
        return -1;
    }
    
    while ((ret = fscanf(fp, "%u %*s %32[0-9A-Fa-f] %*x %*x %*x\n", &idx, addr)) != EOF) {
        if (ret == 2) {
            if (if_index == idx) {
                if (strncmp(addr, ALL_NODES_MCAST, sizeof(addr)) == 0) {
                    allnodes_ok = 1;
                }
            }
        }
    }

    fclose(fp);

    if (!allnodes_ok) {
        if_name = (char *)malloc(IF_NAMESIZE * sizeof(char));
        memset(if_name, 0, IF_NAMESIZE * sizeof(char));
        if_indextoname(if_index, if_name);
        syslog(LOG_LOCAL0|LOG_DEBUG, "[check_allnodes_membership] resetting ipv6-allnodes membership on %s(%d)", if_name, if_index);
        setup_allnodes_membership(sock, if_index);
    }

    return 0;
}

/**
 * @brief �V�O�i���n���h��
 *
 * SIGTERM�Ȃǂ��L���b�`����n���h���B
 * @param sig Signal Number
 * @param si �V�O�i�����\����
 * @param context ���[�U�[�R���e�L�X�g�\����
 */
static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	UNUSED(si);
	UNUSED(context);
	
	switch (sig) {
	case SIGINT:
	    srv_shutdown = 1;
	    //fprintf(stderr, "sigint\n");
	    break;
	default:
	    break;
	}
}

/**
 * @brief �O���[�o���ϐ��̏�����
 *
 * �e��p�����[�^��ۑ����Ă���O���[�o���ϐ�������������B
 */
static void init_parameters() {
    memset(wlan_interface, 0, 5);
    strncpy(wlan_interface, WLAN_INTERFACE, sizeof(WLAN_INTERFACE));
    
    memset(fifo_path, 0, sizeof(fifo_path));
    strncpy(fifo_path, FIFOPATH, sizeof(FIFOPATH));
    
    waiting_time = WAITING_TIME;
    udp_port = UDP_PORT_NUMBER;
}

/**
 * @brief �g�p�@����
 *
 * �R�}���h���C�������̐�����\�����ďI������B
 */
static void usage() {
    fprintf(stderr, "Usage: stamd [options]\n");
    fprintf(stderr, "where options are:\n");
    fprintf(stderr, "  -f fifo_path : Path to FIFO. (%s)\n", FIFOPATH);
    fprintf(stderr, "  -h : Show this message and exit.\n");
    fprintf(stderr, "  -i wlan_interface : WLAN Interface to use. (%s)\n", WLAN_INTERFACE);
    fprintf(stderr, "  -n : Not daemonize.\n");
    fprintf(stderr, "  -p port : UDP port number. (%d)\n", UDP_PORT_NUMBER);
    fprintf(stderr, "  -t waiting_time : Waiting Time [sec] in DAD. (%d)\n", WAITING_TIME);
    exit(1);
}

/**
 * @brief ���C���֐�
 *
 * ���C���֐��B�������f�[�������A�f�[�^��M�X���b�h���N���B
 * @param argc �R�}���h���C�������̐�
 * @param argv �R�}���h���C�������̔z��
 * @retval 0 0��Ԃ�
 */
int main(int argc, char **argv) {
    pthread_t recv_from_fifo_thread_id; // Locationmw�����FIFO��M�X���b�h
    pthread_t recv_from_udp_thread_id;
    int ret;
    struct sigaction act;
    
    memset(&act, 0, sizeof(act));

    openlog("stamd", LOG_PID, LOG_LOCAL0|LOG_DEBUG);
    printf("STA Management Daemon started...\n");
    syslog(LOG_LOCAL0|LOG_DEBUG, "STA Management Daemon started...");
    
    init_parameters();
    
    while ((ret = getopt(argc, argv, "fhi:np:t:")) != -1) {
        switch (ret) {
        case 'f':
            strncpy(fifo_path, optarg, sizeof(fifo_path) - 1);
            break;
        case 'h':
            usage();
            break;
        case 'i':
            strncpy(wlan_interface, optarg, sizeof(wlan_interface) - 1);
            break;
        case 'n':
            daemonize = 0;
            break;
        case 'p':
            udp_port = atoi(optarg);
            break;
        case 't':
            waiting_time = atoi(optarg);
            break;
        default:
            usage();
        }
    }
    
    if (daemonize) {
        daemon(0, 1);
    }
    
    act.sa_sigaction = sigaction_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    if ((sigaction(SIGINT, &act, NULL)) != 0) {
        fprintf(stderr, "sigaction err: %m");
        printf("STA Management Daemon dying...\n");
        closelog();
        return -1;
    }

    init_temporary_address_status();
    init_udp_socket(recv_from_udp_thread_id);
    
    ret = pthread_create(&recv_from_fifo_thread_id, NULL, recv_from_fifo, (void *)NULL);
    
    pthread_join(recv_from_fifo_thread_id, NULL);
    syslog(LOG_LOCAL0|LOG_DEBUG, "STA Management Daemon dying...");
    printf("STA Management Daemon dying...\n");
    
    closelog();
    return 0;
}
