#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <errno.h>

#define REQ_TYPE_READ 0
#define REQ_TYPE_WRITE 1

#define NUM_SERVERS 3

#define MAX_FILENAME_LEN 50
#define IP_ADDR_LEN 16

#define MAX_READ_WRITE_SZ 2048

#define IP_FILE_PATH "server_ipaddr.txt"
#define MIP_FILE_PATH "mserver_ipaddr.txt"

typedef std::pair<std::string, int> srv_data_t;
std::vector<srv_data_t> ip_to_sock;

typedef struct server_request {
        char ip_addr[IP_ADDR_LEN];         /** IP address of the server **/
        char filename[MAX_FILENAME_LEN];       /** Name of the file **/
        int chunk_id;                   /** ID of the appropriate chunk, ignored in case of write **/
        int offset;                         /** Offset within the chunk, ignored in case of write **/
        int error;                          /** Set to a non-zero value indicating the error that happenned during processing, zero otherwise **/
        size_t size;
        int type;                          /** Type of request (read/write) **/
}srv_req_t;

typedef struct client_request {
        int type;                               /** Type of request (read/write) **/
        char filename[MAX_FILENAME_LEN];                   /** Name of the file **/
        int offset;                             /** Offset within the file (ignored during write, and file creation)**/
        size_t size;                           /** Size of the buffer the client wants to read or write (Max MAX_READ_WRITE_SZ) **/
}msrv_req_t;

const int mserver_port_num=3456;
const int server_port_num=3457;
