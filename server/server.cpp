#include "server.h"

using namespace std;

string get_own_IP() ;
size_t get_file_size(string , int);
int write_to_file(char * , char *, size_t );
bool is_chunk_present(chunks_t , int );
int send_message_to_mserver(int );
int connect_to_server() ;

static void handler(int sig, siginfo_t *si, void *uc) {
        /** Mark the server as unavailable **/
        SEND_DATA_TO_SERVER = true;
}

int main(int argc, const char *argv[]) {
        
        int sock_self_server = -1, yes=1, fdmax = 0, newfd, srv_num, mserver_sock;
        fd_set master; 
        fd_set read_fds;
        struct sockaddr_in my_addr, peer_addr;
        struct sockaddr_in remoteaddr; 
        socklen_t addrlen;
        FILE *file = NULL;
        sigset_t sigmask, empty_mask;
        struct ifaddrs *ifaddr, *ifa;
        string host;
        timer_t tm;
        struct itimerspec its;
        struct sigevent sev;
        bool send_hb = true;
        
        addrlen = sizeof(struct sockaddr_in);
        
        SEND_DATA_TO_SERVER =false;
        
        cout<<endl<<"Type 'p' on terminal to pause sending heartbeat to M-server, 'r' to resume sending"<<endl;
        
         /** Set-up Signal Handler **/
        struct sigaction sa;
        
        sa.sa_flags = 0;
        sa.sa_sigaction = handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
                perror("sigaction()::");
                exit(EXIT_FAILURE);
        }
        
        /** Initially block signal **/
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGRTMIN);
        if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
                perror("sigprocmask()::");
                exit(EXIT_FAILURE);
        }
        sigemptyset(&empty_mask);
        srand(time(NULL));
        
        /** Set up Timer **/
        
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGRTMIN;
        if (timer_create(CLOCK_REALTIME, &sev, &tm) == -1){
                perror("timer_create()");
                exit(EXIT_FAILURE);
        }
        
        /** Init FD Sets **/
        FD_ZERO(&read_fds);
        FD_ZERO(&master);
                
         /** Write own IP address for others to read **/
        host = get_own_IP();
        cout<<"IP address of this Server is " <<host<<endl;
        
        file = fopen ( IP_FILE_PATH, "a+" );
        if (!file) {
                perror( IP_FILE_PATH);
                exit(EXIT_FAILURE);
        }
        if(fprintf(file, "%s\n", host.c_str()) < 0){
                perror("fprintf()::");
                exit(EXIT_FAILURE);
        }
        fclose(file);
        
         /** Create and listen to socket for incoming connections */
        if((sock_self_server = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                        perror("socket()::");
                        exit(EXIT_FAILURE);
        }
        if (setsockopt(sock_self_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                        perror("setsockopt()::");
                        exit(EXIT_FAILURE);
        }
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(port_num);
        my_addr.sin_addr.s_addr = inet_addr(host.c_str());
        memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

        if( bind(sock_self_server, (struct sockaddr *)&my_addr, sizeof my_addr) < 0) {
                perror("bind()::");
                close(sock_self_server);
                exit(EXIT_FAILURE);
        }

        if (listen(sock_self_server, 10) == -1) {
                perror("listen()");
                close(sock_self_server);
                exit(EXIT_FAILURE);
        }
        
        FD_SET(sock_self_server, &master);
        fdmax = max(fdmax, sock_self_server);
        
        FD_SET(STDIN_FILENO, &master);
        fdmax = max(fdmax, STDIN_FILENO);
        
        
        /** Connect to M-server **/
        mserver_sock = connect_to_server();
        if(mserver_sock < 0) {
                cout<<"Could not connect to m-server. Exiting.."<<endl;
                exit(EXIT_FAILURE);
        }
        
        /** Start timer to send hearbeat **/
        its.it_value.tv_sec = HEARTBEAT_PERIOD;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec =  0;
        its.it_interval.tv_nsec = 0;
        if (timer_settime(tm, 0, &its, NULL) == -1) {
                perror("timer_settime()");
                exit(EXIT_FAILURE);
        }
        
        cout<<"Starting to listen incoming data packets/connections"<<endl;
        while(1) {
                read_fds = master;    
                if( pselect(fdmax+1, &read_fds, NULL, NULL, NULL, &empty_mask) == -1) {
                        if(errno != EINTR){
                                perror("select()::");
                                close(sock_self_server); 
                                exit(EXIT_FAILURE);
                        }
                        cout<<"pselect() caught a signal"<<endl;
                }
                
                if(SEND_DATA_TO_SERVER){
                        if(send_hb) {
                                cout<<"Sending data to M-Server"<<endl;
                                send_message_to_mserver(mserver_sock);
                                
                                its.it_value.tv_sec = HEARTBEAT_PERIOD;
                                its.it_value.tv_nsec = 0;
                                its.it_interval.tv_sec =  0;
                                its.it_interval.tv_nsec = 0;
                                if (timer_settime(tm, 0, &its, NULL) == -1) {
                                        perror("timer_settime()");
                                        exit(EXIT_FAILURE);
                                }
                        }
                        SEND_DATA_TO_SERVER = false;
                        continue;
                }
                
                for(int i = 0; i <= fdmax; i++) {
                        if(FD_ISSET(i, &read_fds)) {
                                if( i == sock_self_server) {
                                        cout<<"Received a new Connection"<<endl;
                                        if ((newfd = accept(sock_self_server, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
                                                perror("accept()::");
                                        } else {
                                                FD_SET(newfd, &master); 
                                                fdmax = max(newfd, fdmax);
                                        }
                                } else if(i == STDIN_FILENO) {
                                        char input[10];
                                        int ret;
                                        memset(input, 0, 10);
                                        
                                        cin>>input;
                                        
                                        if(!strncmp(input, "p", 1)) {
                                                cout<<"Pausing Heartbeat"<<endl;
                                                its.it_value.tv_sec = 0;
                                                its.it_value.tv_nsec = 0;
                                                its.it_interval.tv_sec =  0;
                                                its.it_interval.tv_nsec = 0;
                                                if (timer_settime(tm, 0, &its, NULL) == -1) {
                                                        perror("timer_settime()");
                                                        exit(EXIT_FAILURE);
                                                }
                                                send_hb = false;
                                        } else if(!strncmp(input, "r", 1)){
                                                cout<<"Resuming Heartbeat"<<endl;
                                                send_message_to_mserver(mserver_sock);
                                
                                                its.it_value.tv_sec = HEARTBEAT_PERIOD;
                                                its.it_value.tv_nsec = 0;
                                                its.it_interval.tv_sec =  0;
                                                its.it_interval.tv_nsec = 0;
                                                if (timer_settime(tm, 0, &its, NULL) == -1) {
                                                        perror("timer_settime()");
                                                        exit(EXIT_FAILURE);
                                                }
                                                send_hb = true;
                                        } else {
                                                cout<<"Unknown Command"<<endl;
                                        }
                                } else {
                                        /** Server has received a message from one of the clients **/
                                        srv_req_t req; 
                                        char path[MAX_PATH_LEN];
                                        
                                        memset(path, 0, MAX_PATH_LEN);
                                        
                                        cout<<"Received Message from Client"<<endl;
                                        
                                        if( recv(i , &req, sizeof(srv_req_t), MSG_WAITALL) < 0){
                                                perror("recv() receiving request from client");
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        //path_stream<<"./"<<req.filename<<"/"<<req.chunk_id<<"\0";
                                        sprintf(path, "./%s/%d", req.filename, req.chunk_id);
                                        printf("Path is :%s\n", path);
                                        
                                        if(req.type == REQ_TYPE_READ ) {
                                                FILE *fp = NULL;
                                                chunks_t tmp;
                                                size_t fsize = 0, ret;
                                                char *data;
                                                string filename(req.filename);
                                               
                                                fp = fopen(path, "r");
                                                if(!fp) {
                                                       perror(path);
                                                       exit(EXIT_FAILURE);
                                                }
                                                fsize = get_file_size(filename, req.chunk_id);
                                                if(fsize == 0){ //NOTE:This is for debugging and nothing else
                                                       cout<<"ERROR...THE FILE DOES NOT EXIST...THIS SHOULD NOT HAPPEN, TERMINATE NOW AND DEBUG"<<endl;
                                               }
                                               
                                               req.size = min(fsize - req.offset, req.size);
                                               if(fseek(fp, req.offset, SEEK_SET) < 0 ){
                                                       perror("fseek()");
                                                       exit(EXIT_FAILURE);
                                               }
                                               
                                               data = new char [req.size];
                                               memset(data, 0, req.size);
                                               
                                               ret = fread(data, sizeof(char), req.size, fp);
                                               if(!ret || ret != req.size) {
                                                       perror("fread()");
                                                       exit(EXIT_FAILURE);
                                               }
                                               
                                               if(send(i, data, ret, 0) < 0) {
                                                        perror("send() called to send read data to client::");
                                                        exit(EXIT_FAILURE);
                                                }
                                        } else {
                                                FILE *fp = NULL;
                                                char *data;
                                                string filename(req.filename);
                                                        
                                                data = new char[req.size];
                                                memset(data, 0, req.size);
                                                
                                                if( recv(i , data, req.size, MSG_WAITALL) < 0){
                                                        perror("recv() receiving request from client");
                                                        exit(EXIT_FAILURE);
                                                }
                                        
                                                if(file_records.find(filename) == file_records.end()) {
                                                        chunk_t entry;
                                                        chunks_t val;
                                                        /** Create a new file **/
                                                        if(mkdir(req.filename, 0777) < 0){
                                                               perror("mkdir()::");
                                                                exit(EXIT_FAILURE);
                                                        }
                                                        
                                                        printf("Folder %s\n", req.filename);
                                                        
                                                        if(write_to_file(path, data, req.size) < 0){
                                                                cout<<"Error during writing data in file, when file is created first time"<<endl;
                                                                exit(EXIT_FAILURE);
                                                        }
                                                        entry.chunk_id = req.chunk_id;
                                                        entry.size = req.size;
                                                        val.push_back(entry);
                                                        file_records.insert(pair<string, chunks_t>(filename, val));
                                                       
                                                }  else if (!is_chunk_present(file_records.find(filename)->second, req.chunk_id)) {
                                                        chunk_t entry;
                                                        /** Just create a new chunk **/
                                                         if(write_to_file(path, data, req.size) < 0){
                                                                cout<<"Error during writing data in file"<<endl;
                                                                exit(EXIT_FAILURE);
                                                        }
                                                        entry.chunk_id = req.chunk_id;
                                                        entry.size = req.size;
                                                        file_records.find(filename)->second.push_back(entry);
                                                } else {
                                                        size_t remaining_size, fsize;
                                                        chunk_t entry;
                                                        
                                                        fsize = get_file_size(filename, req.chunk_id);
                                                        remaining_size = CHUNK_SIZE - fsize;
                                                        
                                                        if(write_to_file(path, data, min(remaining_size, req.size)) < 0){
                                                                        cout<<"Error during writing data in file"<<endl;
                                                                        exit(EXIT_FAILURE);
                                                        }
                                                        file_records.find(filename)->second.back().size += min(remaining_size, req.size);
                                                        if(remaining_size < req.size) {
                                                                /** Add the surplus data in a new chunk **/
                                                                char newpath[MAX_PATH_LEN];
                                                                
                                                                //newpath_stream <<"./"<<req.filename<<"/"<<(req.chunk_id+1)<<"\0";
                                                                memset(newpath, 0, MAX_PATH_LEN);
                                                                sprintf(newpath, "./%s/%d", req.filename, (req.chunk_id+1));
                                                                if(write_to_file(newpath, data + remaining_size, req.size - remaining_size) < 0){
                                                                        cout<<"Error during writing data in file"<<endl;
                                                                        exit(EXIT_FAILURE);
                                                                }
                                                                entry.chunk_id = req.chunk_id+1;
                                                                entry.size = req.size - remaining_size;
                                                                file_records.find(filename)->second.push_back(entry);
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }
        return 0;
}

int send_message_to_mserver(int sock){

        vector<srv_msg_t> serv_data;
        map<string, chunks_t>::iterator it;
        unsigned int total_chunks;

        for(it = file_records.begin(); it != file_records.end(); it++) {
                chunks_t chunks;
                
                chunks = it->second;
                for(chunks_t::iterator it2 = chunks.begin(); it2  != chunks.end(); it2++) {
                        srv_msg_t entry;
                        
                        memset(entry.filename, 0, MAX_FILENAME_LEN);
                        strncpy(entry.filename, it->first.c_str(), it->first.size());
                        entry.chunk_id = it2->chunk_id;
                        entry.size = it2->size;
                        serv_data.push_back(entry);
                }
        }
        
        total_chunks = serv_data.size();
        
        if(send(sock, &total_chunks, sizeof(unsigned int), 0) < 0) {
                perror("send() size to m-server::");
                return -1;
        }
        if(total_chunks) {
                cout<<"Sending "<<total_chunks<<" elements to M-Server"<<endl;
                if((total_chunks = send(sock, &serv_data[0], sizeof(srv_msg_t) * total_chunks, 0)) < 0) {
                        perror("send() chunks to m-server::");
                        return -1;
                }
        } else {
                cout<<"Nothing to Send"<<endl;
        }
        return total_chunks;
}

size_t get_file_size(string filename, int chunk_id){
        chunks_t tmp;
        
        if(file_records.find(filename) == file_records.end()){
                return 0;
        }
        
        tmp = file_records.find(filename)->second;
        for(chunks_t::iterator it = tmp.begin(); it != tmp.end(); it++) {
                if(it->chunk_id == chunk_id){
                        return it->size;
                }
        }
        return 0;
}

int write_to_file(char *path, char *data, size_t size){
        
        FILE *fp;
        
         if((fp = fopen(path, "a+")) == NULL) {
                        perror("fopen() during file creation::");
                        return -1;
        }
                                                        
        if(!fwrite(data, sizeof(char), size, fp)){
                perror("fwrite() during file creation::");
                return -1;
        }
                                                        
        fclose(fp);
        return 1;
}

bool is_chunk_present(chunks_t tmp, int chunk_id){
         for(chunks_t::iterator it = tmp.begin(); it != tmp.end(); it++) {
                if(it->chunk_id == chunk_id){
                        return true;
                }
         }
         return false;
}

/** Get own IP adress **/
string get_own_IP() {
        struct ifaddrs *ifaddr, *ifa;
        char host[IP_ADDR_LEN];
        int s;
       
        if (getifaddrs(&ifaddr) == -1) {
                perror("getifaddrs");
                exit(EXIT_FAILURE);
        }
        
        memset(host, 0, IP_ADDR_LEN);
        for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if((ifa->ifa_addr->sa_family == AF_INET) && strncmp(ifa->ifa_name, "lo", 2)) {
                        /** Not loopback/assuming only one AF_INET interface other than lo **/
                        s = getnameinfo(ifa->ifa_addr,  sizeof(struct sockaddr_in) , host, IP_ADDR_LEN, NULL, 0, NI_NUMERICHOST);
                        break;
                }
        }
        freeifaddrs(ifaddr);
        
        return string(host, IP_ADDR_LEN);
}

int connect_to_server() {
        int sockfd;
        struct sockaddr_in dest_addr;
        FILE *file = NULL;
        char ip[16];
        
        file = fopen ( MIP_FILE_PATH, "a+" );
        if (!file) {
                perror( MIP_FILE_PATH);
                return -1;;
        }
        if(!fgets ( ip, sizeof (ip), file)) {
                perror("Error reading the IP file");
                return -1;
        }
        sockfd = socket(PF_INET, SOCK_STREAM, 0); // do some error checking!
        dest_addr.sin_family = AF_INET;

        dest_addr.sin_port = htons(mserver_port_num);

        dest_addr.sin_addr.s_addr = inet_addr(ip);
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof dest_addr) < 0){
                perror("ERROR connect()");
                return -1;
        }
        
        return sockfd;
}