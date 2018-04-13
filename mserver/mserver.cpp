#include "mserver.hpp" 

using namespace std;

string get_own_IP() ;
void handle_request (cl_req_t, srv_req_t *);
size_t get_file_size(string);
 void handle_server_data (int, int);

static void handler(int sig, siginfo_t *si, void *uc) {
        /** Mark the server as unavailable **/
        int *sock;
        
        sock = (int *)si->si_value.sival_ptr;

        for(int i = 0; i < NUM_SERVERS; i++) {
                if(*sock == server_avail_table[i].first) {
                        server_avail_table[i].second = false;
                        cout<<"Timer for server at IP "<<get_ip_from_sock(server_avail_table[i].first)<<" has expired"<<endl;
                        return;
                }
        }
}

int main() {
        int  yes=1, fdmax = 0, newfd,  ret, sock_self_server = -1, num_servers_left = NUM_SERVERS;
        fd_set master; 
        fd_set read_fds;
        struct sockaddr_in my_addr;
        struct sockaddr_in remoteaddr; 
        sigset_t sigmask, empty_mask;
        socklen_t addrlen;
        vector<timer_t> timerid;
        FILE *file = NULL;
        string host;
        
        /** Set-up Signal Handler **/
        struct sigaction sa;
        
        addrlen = sizeof(struct sockaddr_in);
        
        sa.sa_flags = SA_SIGINFO;
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
        
        FD_ZERO(&read_fds);
        FD_ZERO(&master);
        
        host = get_own_IP();
        cout<<"IP address of M-Server is " <<host<<", "<<host.size()<<endl;
        
        /** Write own IP address for others to read **/
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
        if((sock_self_server = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
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
        
        cout<<"Done all initializations.. Waiting  connections/messages"<<endl;
        
        while(1){
                read_fds = master;    
                if( pselect(fdmax+1, &read_fds, NULL, NULL, NULL, &empty_mask) == -1) {
                        if(errno != EINTR){
                                perror("select()::");
                                close(sock_self_server); 
                                exit(EXIT_FAILURE);
                        }  
                        cout<<"pselect() caught a signal"<<endl;
                        continue;
                }
                
                for(int i = 0; i <= fdmax; i++) {
                        if(FD_ISSET(i, &read_fds)) {
                                if( i == sock_self_server) {
                                        /** We have a new incoming connection **/
                                        cout<<"Received a New Connection"<<endl;
                                        if ((newfd = accept(sock_self_server, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
                                                perror("accept():");
                                        } else {
                                                FD_SET(newfd, &master); 
                                                fdmax = max(newfd, fdmax);
                                                if(num_servers_left) {
                                                        /** Incoming connection is a server **/
                                                        srv_data_t server;
                                                        timer_t tm;
                                                        struct sigevent sev;
                                                        struct itimerspec its;
                                                        int *fd = new int;
                                                        
                                                        server = make_pair(newfd, string (inet_ntoa(remoteaddr.sin_addr)));
                                                        
                                                        sev.sigev_notify = SIGEV_SIGNAL;
                                                        sev.sigev_signo = SIGRTMIN;

                                                        sev.sigev_value.sival_ptr = new int;
                                                        memcpy(sev.sigev_value.sival_ptr, &newfd, sizeof(int));

                                                        if (timer_create(CLOCKID, &sev, &tm) == -1){
                                                                perror("timer_create()");
                                                                exit(EXIT_FAILURE);
                                                        }
                                                       
                                                        its.it_value.tv_sec = MAX_TIMEOUT_SEC;
                                                        its.it_value.tv_nsec = 0;
                                                        its.it_interval.tv_sec =  0;
                                                        its.it_interval.tv_nsec = 0;
                                                        if (timer_settime(tm, 0, &its, NULL) == -1) {
                                                                perror("timer_settime()");
                                                                exit(EXIT_FAILURE);
                                                        }

                                                        timerid.push_back(tm);
                                                        sdata.push_back(server);
                                                        server_avail_table.push_back(make_pair(newfd, true));
                                                        num_servers_left--;
                                                        cout<<"Connection received from server "<<server.second<<" on socket "<<newfd<<endl;
                                                } else {
                                                        cout<<"Connection received from client on socket "<<newfd<<endl;
                                                }
                                        }
                                } else {
                                        if(is_server(i)) {
                                                /** Keep-alive message from server **/
                                                int srv_index = -1;
                                                struct itimerspec its;
                                                
                                                for(int j = 0; j < NUM_SERVERS; j++ ) {
                                                        if(sdata[j].first == i) {
                                                                srv_index = j;
                                                                if(server_avail_table[j].first != sdata[j].first) {
                                                                        cout<<"**********DISASTER********"<<endl;
                                                                }
                                                                if(!server_avail_table[j].second) {
                                                                        cout<<"Server "<<get_ip_from_sock(server_avail_table[j].first)<<" is running again"<<endl;
                                                                        server_avail_table[j].second = true;
                                                                }
                                                                break;
                                                        }
                                                }
                                                /** Handle Server Keep Alive Message **/
                                                handle_server_data(srv_index, i);
                                                
                                                /** Restart the timer **/
                                                its.it_value.tv_sec = MAX_TIMEOUT_SEC;
                                                its.it_value.tv_nsec = 0;
                                                its.it_interval.tv_sec =  0;
                                                its.it_interval.tv_nsec = 0;

                                                if (timer_settime( timerid[srv_index], 0, &its, NULL) == -1) {
                                                        perror("timer_settime()");
                                                        exit(EXIT_FAILURE);
                                                }
                                        } else {
                                                /** File system access request from client **/
                                                 int num_bytes = 0, ret = -1;
                                                 cl_req_t req;
                                                 srv_req_t sreq;
                                                
                                                 num_bytes = recv(i, &req, sizeof(cl_req_t), MSG_WAITALL);
                                                 printf("Recieved a file access request from client for file %s\n", req.filename);
                                                 
                                                 switch (num_bytes) {
                                                        case -1:
                                                                perror("recv() from client:: ");
                                                                exit(EXIT_FAILURE);
                                                        case 0:
                                                                cout<<"Client has shut down the socket"<<endl;
                                                                close(i);
                                                                FD_CLR(i, &master);
                                                                ret = 0;
                                                                break;
                                                        case sizeof(cl_req_t):
                                                               
                                                                handle_request(req, &sreq);
                                                                cout<<"Done processing Client request. Client should access "<<sreq.ip_addr<< ", file"<<sreq.filename<<endl;
                                                                break;
                                                        default:
                                                                cout<<"Incomplete message received, this should not happen"<<endl;
                                                                exit(EXIT_FAILURE);
                                                }
                                                
                                                if(send(i, &sreq, sizeof(srv_req_t), 0) < 0) {
                                                        perror("send() called to send request to peers::");
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                }
                        }
                }
        }
        return 0;
}

void handle_server_data (int index, int sock) {
        int num_entries = 0;
        bool flag ;
        
        if( recv(sock , &num_entries, sizeof(int), MSG_DONTWAIT) < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        cout<<"Reading Data Will block..."<<endl;
                        return;
                }
                perror("recv() receiving request from server");
                exit(EXIT_FAILURE);
        }
        if(!num_entries){
                cout<<"No chunks with "<<get_ip_from_sock(sock)<<endl;
                return;
        }
        
        
        for(int i = 0; i < num_entries; i++) {
                srv_msg_t msg;
                map<string, chunks_t>::iterator it;
                
                
                flag  = false;
                
                if( recv(sock , &msg, sizeof(srv_msg_t), MSG_WAITALL) < 0){
                        perror("recv() receiving request from server");
                        exit(EXIT_FAILURE);
                }
                string filename(msg.filename);
                it = db.find(filename);
                
                if(it == db.end()) {
                        cout<<"Server has file named \""<<filename<<"\" which M-Server does not know about"<<endl;
                        continue;
                }
                
                for(chunks_t::iterator j = it->second.begin();  j !=  it->second.end(); j++) {
                        if(j->chunk_id == msg.chunk_id && j->size == msg.size && sock == j->serv_sock) {
                                cout<<" Sync of chunk "<<msg.chunk_id<<" belonging to file "<<filename<<" done!!"<<endl;
                                flag = true;
                                break;
                        } 
                }
                
                if(!flag) {
                        cout<<endl<<"Chunk data mismatch between server and M-server for filename \""<<filename<<"\""<<endl;
                        cout<<"Recieved Chunk ID: "<<msg.chunk_id<<", File Size: "<<msg.size<<endl;
                        cout<<"For filename "<<filename<<", server has the following chunks:"<<endl;
                        for(chunks_t::iterator j = it->second.begin();  j !=  it->second.end(); j++) {
                                cout<<"Chunk ID: "<<j->chunk_id<<", File Size: "<<j->size<<endl;
                        }
                }
        }
}

size_t get_file_size(string filename) {
        
        chunks_t list;
        
        if(db.find(filename) == db.end()) {
                cout<<"File not found";
                return 0;
        }
        
        list = db.find(filename)->second; 
        return ((list.size() - 1) * CHUNK_SIZE) + list.back().size;
}

void handle_request (cl_req_t creq, srv_req_t *sreq) {
        string filename(creq.filename);
        
        sreq->error = 0;
        memset(sreq->filename, 0, MAX_FILENAME_LEN);
        strncpy(sreq->filename, creq.filename, strlen(creq.filename));
        sreq->type = creq.type;
        
        map<string, chunks_t>::iterator it = db.find(filename);
        if(creq.type == REQ_TYPE_READ){
                int chunk_index = -1;
                size_t filesize;
                        
                if(it == db.end()) {
                        cout<<"File "<<filename<<" does not exist"<<endl;
                        sreq->error = ENOENT;
                        return;
                }
                filesize = get_file_size(filename); 
                if(creq.offset > filesize) {
                        sreq->error = EINVAL;
                        cout<<"Offset supplied by client for file "<<filename<<" is invalid"<<endl;
                        return;
                }
                chunk_index = creq.offset / CHUNK_SIZE;
                        
                if(!is_serv_available(it->second[chunk_index].serv_sock)){
                        sreq->error = EHOSTDOWN;
                        cout<<"Server is not available for the file "<<filename<<endl;
                        return;
                }
                sreq->chunk_id = it->second[chunk_index].chunk_id;
                sreq->offset = creq.offset % CHUNK_SIZE;
                sreq->size = min(creq.size, filesize - sreq->offset);
                try {
                        string st = get_ip_from_sock(it->second[chunk_index].serv_sock).c_str();
                        memset(sreq->ip_addr, 0, IP_ADDR_LEN);
                        strncpy(sreq->ip_addr, st.c_str(), st.size());
                } catch(string &e) {
                        cout<<"Socket number is invalid for some reason"<<endl;
                }
        } else {
                sreq->size = creq.size;

                if( (it == db.end()) || (it->second.back().size == CHUNK_SIZE) ) {
                        chunk_t entry;
                        /** A new chunk has to be created **/
                        
                        /** TODO: Ask instructor/TA whether the new chunk has to be stored in DB now 
                         *   or later when server sends heartbeat, what if client makes req to this chunk 
                         *   before M-server receives the heartbeat.
                         **/
                        int index, sock, i;
                        
                        index = rand() % NUM_SERVERS;
                        
                        cout<<"A new chunk has to be created "<<index<<endl;
                        for( i = 0; i < NUM_SERVERS; i++){
                                if(is_serv_available(sdata[(index+i) % NUM_SERVERS].first)){
                                        string st = sdata[(index+i) % NUM_SERVERS].second;
                                        
                                        memset(sreq->ip_addr, 0, IP_ADDR_LEN);
                                        strncpy(sreq->ip_addr, st.c_str(), st.size());
                                        sock = sdata[(index+i) % NUM_SERVERS].first;
                                        break;
                                }
                        }
                        if( i == NUM_SERVERS) {
                                sreq->error = EHOSTDOWN;
                                cout<<"None of the servers are available for this operation "<<endl;
                                return;
                        }
                        
                        entry.serv_sock = sock;
                        entry.size = creq.size;

                        if(it == db.end()) {
                                chunks_t val;
                                entry.chunk_id = 0;
                                val.push_back(entry);
                                db.insert(pair<string, chunks_t>(filename, val));
                        } else {
                                entry.chunk_id = it->second.back().chunk_id + 1;
                                it->second.push_back(entry);
                                
                        }
                        sreq->chunk_id = entry.chunk_id;
                       
                } else {
                        if(!is_serv_available(it->second.back().serv_sock)){
                                sreq->error = EHOSTDOWN;
                                cout<<"Server is not available for the file "<<filename<<endl;
                                return;
                        }
                        
                        sreq->chunk_id = it->second.back().chunk_id;
                        if((CHUNK_SIZE - it->second.back().size) < creq.size){
                                /** A new chunk must be created **/
                                chunk_t entry;

                                
                                entry.chunk_id = it->second.back().chunk_id + 1;
                                entry.serv_sock = it->second.back().serv_sock;
                                entry.size = creq.size - (CHUNK_SIZE - it->second.back().size);
                                it->second.back().size = CHUNK_SIZE;
                                it->second.push_back(entry);
                                
                        } else {
                                it->second.back().size += creq.size;
                        }
                        try {
                                string st = get_ip_from_sock(it->second.back().serv_sock).c_str();
                                
                                memset(sreq->ip_addr, 0, IP_ADDR_LEN);
                                strncpy(sreq->ip_addr, st.c_str(), st.size());
                        } catch(string &e) {
                                cout<<"Socket number is invalid for some reason"<<endl;
                        }
                } 
        }
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