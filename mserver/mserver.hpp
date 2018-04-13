#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sched.h>
 #include <sys/select.h>
 #include <ifaddrs.h>
 #include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#define NUM_SERVERS 3
#define CHUNK_SIZE 8192

#define MAX_FILENAME_LEN 50

#define IP_ADDR_LEN 16

#define REQ_TYPE_READ 0
#define REQ_TYPE_WRITE 1

#define IP_FILE_PATH "mserver_ipaddr.txt"

#define CLOCKID CLOCK_REALTIME
#define MAX_TIMEOUT_SEC 15
#define SRV_MSG_PERIOD 5
#define MAX_READ_WRITE_SZ 2048

typedef std::pair<int, std::string> srv_data_t;
typedef std::pair<int, bool> srv_avail_t;

typedef struct file_chunk_data {
        unsigned int chunk_id;          /** Index of chunk in the file itself **/
        unsigned int serv_sock;          /** The server in which this chunk is stored **/
        size_t size;                             /** Size of the chunk, max CHUNK_SIZE **/
}chunk_t;

typedef struct client_request {
        int type;                               /** Type of request (read/write) **/
        char filename[MAX_FILENAME_LEN];        /** Name of the file **/
        int offset;                             /** Offset within the file (ignored during write, and file creation)**/
        size_t size;                           /** Size of the buffer the client wants to read or write (Max MAX_READ_WRITE_SZ) **/
}cl_req_t;

typedef struct server_request {
        char ip_addr[IP_ADDR_LEN];        /** IP address of the server **/
        char filename[MAX_FILENAME_LEN];       /** Name of the file **/
        int chunk_id;                   /** ID of the appropriate chunk, ignored in case of write **/
        int offset;                         /** Offset within the chunk, ignored in case of write **/
        int error;                          /** Set to a non-zero value indicating the error that happenned during processing, zero otherwise **/
        size_t size;                        /** Size of the buffer the server should allow to read or write (Max MAX_READ_WRITE_SZ) **/
        int type;                               /** Type of request (read/write) **/
}srv_req_t;

typedef struct server_msg {
        char filename[MAX_FILENAME_LEN];
        int chunk_id;
        size_t size;
}srv_msg_t;

typedef std::vector<chunk_t> chunks_t;

std::map<std::string, chunks_t> db;

std::vector<srv_data_t> sdata;                       /** A mapping between server socket no. and IP address **/         
std::vector<srv_avail_t> server_avail_table;  /** A mapping between server socket no. and its availability **/


const int port_num=3456;

/** Check if socket belongs to server **/
inline bool is_server(int sock) {
        for(int i = 0; i < NUM_SERVERS; i++) {
                if(sdata[i].first == sock)
                        return true;
        }
        return false;
}

inline std::string get_ip_from_sock(int sock){
        for(int i = 0; i < NUM_SERVERS; i++) 
                if(sdata[i].first == sock)
                        return sdata[i].second;
        
        throw std::string("Socket Number invalid!!");
}

inline bool is_serv_available(int sock){ 
        for(int i  = 0; i < NUM_SERVERS; i++) 
                if(server_avail_table[i].first == sock)
                        return server_avail_table[i].second;
        
        return false;
}