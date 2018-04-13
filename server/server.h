#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sched.h>
#include <sys/select.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <queue>
#include <vector>
#include <map>
#define MAX_PATH_LEN 100
#define IP_ADDR_LEN 16
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#define CHUNK_SIZE 8192
#define MAX_FILENAME_LEN 50
#define IP_ADDR_LEN 16

#define REQ_TYPE_READ 0
#define REQ_TYPE_WRITE 1

#define IP_FILE_PATH "../server_ipaddr.txt"
#define MIP_FILE_PATH "../mserver_ipaddr.txt"

#define HEARTBEAT_PERIOD 5

typedef struct file_chunk_data {
        unsigned int chunk_id;          /** Index of chunk in the file itself **/
        size_t size;                             /** Size of the chunk, max CHUNK_SIZE **/
}chunk_t;
typedef std::vector<chunk_t> chunks_t;

typedef struct server_request {
        char ip_addr[IP_ADDR_LEN];           /** IP address of the server **/
        char filename[MAX_FILENAME_LEN];       /** Name of the file **/
        int chunk_id;                   /** ID of the appropriate chunk, ignored in case of write **/
        int offset;                         /** Offset within the chunk, ignored in case of write **/
        int error;                          /** Set to a non-zero value indicating the error that happenned during processing, zero otherwise **/
        size_t size;
        int type;                          /** Type of request (read/write) **/
}srv_req_t;

/** The server sends messages of this type to client **/
typedef struct server_msg {
        char filename[MAX_FILENAME_LEN];
        int chunk_id;
        size_t size;
}srv_msg_t;

std::map<std::string, chunks_t> file_records;

const int mserver_port_num=3456;
const int port_num=3457;
static bool SEND_DATA_TO_SERVER;