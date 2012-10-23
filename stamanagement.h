/**
 * @file stamanagement.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA Management Daemon
 * STA�̃Z�b�g�A�X�V�Ȃǂ��s���f�[����
 */

#ifndef _STAMANAGEMENT_H
#define _STAMANAGEMENT_H

#define FIFOPATH "/tmp/sta.fifo"
#define WLAN_INTERFACE "ath0"
#define WAITING_TIME 10 ///< second
#define UDP_PORT_NUMBER 5003 ///< GPSR��DEFAULT_DAEMON_PORT�ADEFAULT_OAM_PORT�̎�
#define UDP_RECV_BUF_SIZE 512
#define IN6ADDR_MC_LINKLOCAL_INIT { { { 0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1 } } }
#define AREQ_PACKET_SIZE 160
#define AREP_PACKET_SIZE 160
#define ALL_NODES_MCAST "ff020000000000000000000000000001"
#define PATH_PROC_NET_IGMP6 "/proc/net/igmp6"

// �R���p�C���̌x����}���邽�߂Ɏg��
#define UNUSED(x) ((void)(x))

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
 * @brief �A�h���X�̏�Ԃ�\�����邽�߂̗񋓌^
 *
 * DAD���Ă���r���A�d������A�d���Ȃ���3��Ԃ�\������B
 */
typedef enum _address_status {
    DAD,
    DUPLICATE,
    NOT_DUPLICATE
} address_status;

/**
 * @brief �p�P�b�g�̃^�C�v��\�����邽�߂̗񋓌^
 *
 * AREQ�AAREP�Ȃǃp�P�b�g�̃^�C�v
 */
typedef enum _packet_type {
    AREQ,
    AREP
} packet_type;

/**
 * @brief ���U�����U�s��
 */
typedef double matrix_t[4];

/**
 * @brief �~�h���E�F�A����̏o��
 * 
 */
typedef struct _PositionOut {
	long unsigned int index; ///< index
	int nodeid[16]; ///< �m�[�hID
  	time_t time; ///< time
  	double lat; ///< latitude
  	double lon; ///< longitude
  	double alt; ///< altitude
  	matrix_t error; ///< ���U�����U�s��
  	double radio_range; ///< �������a
} PositionOut;

/**
 * @brief recv_from_udp_child�֐��̈���
 *
 * recv_from_udp_child�֐��̈����̌^�B
 * ���M���A�h���X�ƃf�[�^���i�[����Ă���o�b�t�@�̃A�h���X�B
 */
typedef struct _pthreadarg_addr_buf {
    struct sockaddr_storage fromaddr;
    char *buf;
    int usedlen;
} pthreadarg_addr_buf;

/**
 * @brief ���蓖�Ė�������Ԃ̉��A�h���X
 * 
 * ���͂��ꂽ�ʒu���Ɋ�Â�����������
 * �܂�DAD�̌��ʂ��o���A���蓖�Ă�
 * �������Ă��Ȃ����̃A�h���X
 */
typedef struct _temporary_address_status {
    struct sockaddr_in6 address; ///< ���̃A�h���X
    time_t generated_time;
    pthread_mutex_t mutex;
    address_status flag; ///< �d�����Ă��邩�ǂ����B0�ŏd���Ȃ��A1�ł���B
} temporary_address_status;

/**
 * @brief AREP�p�P�b�g��16����31�r�b�g��
 *
 * AREP�p�P�b�g��16����31�r�b�g�ځA1�r�b�g��AREP_FLAG��
 * 15�r�b�g�̗\��̈悩��Ȃ�B
 */
typedef struct _arep_flag_reserved {
    unsigned arep_flag : 1;
    unsigned reserved : 15;
} __attribute__((packed)) arep_flag_reserved;

int daemonize = 1;
char fifo_path[256];
char wlan_interface[5];
int udp_port = 0;
int waiting_time = 0;
int sockfd; ///< UDP��M�\�P�b�g�̃f�B�X�N���v�^
temporary_address_status temp_address; ///< ���蓖�Ė�������Ԃ̉��A�h���X
static struct in6_addr in6addr_linklocalmulticast = IN6ADDR_MC_LINKLOCAL_INIT;

static volatile sig_atomic_t srv_shutdown = 0;

static int add_sta(struct sockaddr_in6 *newsta);
static int allocation_request_start(struct sockaddr_in6 newsta);
static void allocation_request_timeout(void);
static int check_allnodes_membership(int sock, unsigned int if_index);
static int decode_from_sta(struct in6_addr *sta, PositionOut *po);
static int delete_sta(struct sockaddr_in6 *oldsta);
static int encode_to_sta(PositionOut po, struct in6_addr *newsta);
static int get_socket_for_afinet6();
static int in6_addr_equal(const struct in6_addr *a, const struct in6_addr *b);
static void init_parameters(void);
static void init_temporary_address_status(void);
static int init_udp_socket(pthread_t recv_from_udp_thread_id);
static int is_inside_valid_range(const PositionOut * const real, const PositionOut * const decoded);
static int setup_allnodes_membership(int sock, unsigned int if_index);
static void sigaction_handler(int sig, siginfo_t *si, void *context);
static void usage(void);
inline static double lat2y(double lat);
inline static double lon2x(double lon, double lat);
inline static int packet_type_is_areq(char *buf);
inline static int packet_type_is_arep(char *buf);
inline static int is_duplicate(char *buf);

void *recv_from_fifo(void *arg);
void *recv_from_udp(void *arg);
void *recv_from_udp_child(void *arg);

#endif
