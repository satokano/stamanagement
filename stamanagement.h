/**
 * @file stamanagement.h
 * @author Satoshi OKANO <okano@mcl.iis.u-tokyo.ac.jp>
 * @brief STA Management Daemon
 * STAのセット、更新などを行うデーモン
 */

#ifndef _STAMANAGEMENT_H
#define _STAMANAGEMENT_H

#define FIFOPATH "/tmp/sta.fifo"
#define WLAN_INTERFACE "ath0"
#define WAITING_TIME 10 ///< second
#define UDP_PORT_NUMBER 5003 ///< GPSRのDEFAULT_DAEMON_PORT、DEFAULT_OAM_PORTの次
#define UDP_RECV_BUF_SIZE 512
#define IN6ADDR_MC_LINKLOCAL_INIT { { { 0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1 } } }
#define AREQ_PACKET_SIZE 160
#define AREP_PACKET_SIZE 160
#define ALL_NODES_MCAST "ff020000000000000000000000000001"
#define PATH_PROC_NET_IGMP6 "/proc/net/igmp6"

// コンパイラの警告を抑えるために使う
#define UNUSED(x) ((void)(x))

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
 * @brief アドレスの状態を表現するための列挙型
 *
 * DADしている途中、重複あり、重複なしの3状態を表現する。
 */
typedef enum _address_status {
    DAD,
    DUPLICATE,
    NOT_DUPLICATE
} address_status;

/**
 * @brief パケットのタイプを表現するための列挙型
 *
 * AREQ、AREPなどパケットのタイプ
 */
typedef enum _packet_type {
    AREQ,
    AREP
} packet_type;

/**
 * @brief 分散共分散行列
 */
typedef double matrix_t[4];

/**
 * @brief ミドルウェアからの出力
 * 
 */
typedef struct _PositionOut {
	long unsigned int index; ///< index
	int nodeid[16]; ///< ノードID
  	time_t time; ///< time
  	double lat; ///< latitude
  	double lon; ///< longitude
  	double alt; ///< altitude
  	matrix_t error; ///< 分散共分散行列
  	double radio_range; ///< 無線半径
} PositionOut;

/**
 * @brief recv_from_udp_child関数の引数
 *
 * recv_from_udp_child関数の引数の型。
 * 送信元アドレスとデータが格納されているバッファのアドレス。
 */
typedef struct _pthreadarg_addr_buf {
    struct sockaddr_storage fromaddr;
    char *buf;
    int usedlen;
} pthreadarg_addr_buf;

/**
 * @brief 割り当て未完了状態の仮アドレス
 * 
 * 入力された位置情報に基づき生成したが
 * まだDADの結果が出ず、割り当ては
 * 完了していない仮のアドレス
 */
typedef struct _temporary_address_status {
    struct sockaddr_in6 address; ///< 仮のアドレス
    time_t generated_time;
    pthread_mutex_t mutex;
    address_status flag; ///< 重複しているかどうか。0で重複なし、1であり。
} temporary_address_status;

/**
 * @brief AREPパケットの16から31ビット目
 *
 * AREPパケットの16から31ビット目、1ビットのAREP_FLAGと
 * 15ビットの予約領域からなる。
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
int sockfd; ///< UDP受信ソケットのディスクリプタ
temporary_address_status temp_address; ///< 割り当て未完了状態の仮アドレス
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
