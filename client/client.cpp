#include "client.h"

using namespace std;

int get_sock_from_ip(char * );
int connect_to_server(char [], int);
int populate_data(char * , char *, size_t );

int main(int argc, const char *argv[]) {
        
        int srv_sock[NUM_SERVERS], msrv_sock, i, ret;
        FILE *file = NULL;
        char ipaddr[IP_ADDR_LEN]; 
        ifstream inFile;
        msrv_req_t msrvreq;
        string fn;
        
        if(argc < 2){
                cout<<"Usage: ./client <input file name>"<<endl;
                exit(EXIT_FAILURE);
        }
        
         srand(time(NULL));
        
        /** Connect to servers **/
        file = fopen ( IP_FILE_PATH, "r" );
        if (!file) {
                perror( IP_FILE_PATH);
                return -1;
        }
        i = 0 ;
        memset(ipaddr, 0, IP_ADDR_LEN);
        while((fgets ( ipaddr, sizeof (ipaddr), file) ) && (i < NUM_SERVERS)) {
                int ret;
                
                
                if(ipaddr[strlen(ipaddr) - 1] == '\n') {
                       /* Add EOL character */
                        ipaddr[strlen(ipaddr) - 1] = '\0';
                }
                
                if( (ret = connect_to_server(ipaddr, server_port_num) ) < 0 ){
                        cout<<"Error in connecting with server "<<i<<endl;
                        exit(EXIT_FAILURE);
                }
                
                 ip_to_sock.push_back(make_pair(string(ipaddr), ret));
                 memset(ipaddr, 0, IP_ADDR_LEN);
        }
        fclose(file);
        
        /** Connect to M-Server **/
        file = fopen ( MIP_FILE_PATH, "r" );
        if (!file) {
                perror( MIP_FILE_PATH);
                return -1;
        }
        if(!fgets ( ipaddr, sizeof (ipaddr), file)){
                perror("Error opening file containing M-Server IP");
                exit(EXIT_FAILURE);
        }
        if(ipaddr[strlen(ipaddr) - 1] != '\0') {
                /* Add EOL character */
                ipaddr[strlen(ipaddr) - 1] = '\0';
        }
        msrv_sock = connect_to_server(ipaddr, mserver_port_num);
        if(msrv_sock < 0){
                cout<<"Error in connecting with M-Server"<<endl;
                exit(EXIT_FAILURE);
        }
        fclose(file);
        
        
        inFile.open(argv[1]);
        if (!inFile){
                cout<<"Error opening Input file"<<endl;
                exit(EXIT_FAILURE);
        }
        /** The format is : <Filename> <Request Type> <Offset> <Size> **/
        while(inFile >> fn >>  msrvreq.type >> msrvreq.offset >> msrvreq.size ){
                
                /** Get meta Data from M-server **/
                srv_req_t rec;
                int sock;
                char *data = NULL;
                
                memset(msrvreq.filename, 0, MAX_FILENAME_LEN);
                strncpy(msrvreq.filename, fn.c_str(), fn.size());
                
                cout<<"Requesting to access file "<<msrvreq.filename <<" at "<<msrvreq.offset  <<endl;
                
                if((ret = send(msrv_sock, &msrvreq, sizeof(msrv_req_t), 0)) < 0) {
                        perror("send() request to m-server::");
                        exit(EXIT_FAILURE);
                }
                
                if( recv(msrv_sock , &rec, sizeof(srv_req_t), MSG_WAITALL) < 0){
                        perror("recv() receiving request from m-server");
                        exit(EXIT_FAILURE);
                }
                if(rec.error) {
                        cout<<"Error occured in server"<<endl;
                        sleep(5);
                        continue;
                }
                
                data = new  char[rec.size];
                memset(data, 0, rec.size);
                
                if((sock = get_sock_from_ip(rec.ip_addr) )< 0) {
                        cout<<"IP:"<<rec.ip_addr<<" sent by M-Server not found"<<endl;
                         exit(EXIT_FAILURE);
                }
                
                cout<<"Read/Write done to "<< rec.ip_addr<<" for file "<<rec.filename<<endl;
                
                if((ret = send(sock, &rec, sizeof(srv_req_t), 0)) < 0) {
                        perror("send() request to server::");
                        exit(EXIT_FAILURE);
                }
                
                if(msrvreq.type == REQ_TYPE_READ){
                        if( recv(sock , data, rec.size, MSG_WAITALL) < 0){
                                perror("recv() receiving request from m-server");
                                exit(EXIT_FAILURE);
                        }
                        
                        printf("Read the following data:\n%s\n\n", data);
                } else {
                        cout<<"Sending data for write to actual server"<<endl;
                        /**  Populate data to write**/
                        if( (ret = populate_data(msrvreq.filename, data, rec.size)) < 0) {
                                cout<<"Error in reading file to populate data"<<endl;
                                exit(EXIT_FAILURE);
                        }
                
                        /** Send data for Write */
                        if((ret = send(sock, data, /*rec.size*/ret, 0)) < 0) {
                                perror("send() request to server::");
                                exit(EXIT_FAILURE);
                        }
                        cout<<"Wrote "<<ret<<" bytes of data. Was supposed to write "<<rec.size <<" bytes of data"<<endl;
                }

                delete data;
                /** Sleep for 5 seconds before making the next request **/
                sleep(5);
        }
        inFile.close();
        return 0;
}

int populate_data(char *filename, char *buf, size_t size) {
        
        unsigned int offset;
        struct stat buffer;
        int status, ret;
        FILE *fp = NULL;
        
        status = stat(filename, &buffer);
        if(status != 0){
                perror("stat()");
                return -1;
        }
        offset =  rand() % (buffer.st_size - size - 1);
        
        fp = fopen(filename, "r");
        if(!fp) {
                perror(filename);
                return -1;
        }
        
        if(fseek(fp, offset, SEEK_SET) < 0 ){
                perror("fseek()");
                return -1;
        }
        
        ret = fread(buf, sizeof(char), size, fp);
        if(!ret || ret != size) {
                cout<<"Read "<<ret<<" bytes of data instead of "<<size<<" bytes"<<endl;
                perror("fread()");
                 return -1;
        }
        
        fclose(fp);
        return ret;
}

int get_sock_from_ip(char *ip) {
        for(int i = 0; i < NUM_SERVERS; i++){
                if(!strncmp(ip, ip_to_sock[i].first.c_str(), strlen(ip))) {
                        return ip_to_sock[i].second;
                }
        }
        return -1;
}

int connect_to_server(char ipaddr[], int port_num) {
        int sockfd;
        struct sockaddr_in dest_addr;

        cout<<"Connecting to"<<ipaddr<<endl;
        sockfd = socket(PF_INET, SOCK_STREAM, 0); // do some error checking!
        dest_addr.sin_family = AF_INET;

        dest_addr.sin_port = htons(port_num);

        dest_addr.sin_addr.s_addr = inet_addr(ipaddr);
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof dest_addr) < 0){
                perror("ERROR connect()");
                return -1;
        }

        return sockfd;
}