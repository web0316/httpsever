#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<cassert>
#include<sys/epoll.h>
#include<stdio.h>
#include<vector>
#include<unordered_map>
#include<stdlib.h>
#include<signal.h>
#include<iostream>
#include"connection.h"
#include"threadpool.h"

using std::cout;
using std::endl;

extern void add_fd(int epollfd,int fd,bool keep_alive);
extern void remove_fd(int epollfd,int fd);

int start_server(const char*ip,int port){
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	int listenfd = socket(PF_INET,SOCK_STREAM,0);
    address.sin_family = PF_INET;
	address.sin_port = htons(port);
	//cout<<htons(port)<<endl;
	inet_pton(AF_INET,ip,&address.sin_addr);
	int ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    //cout<<address.sin_addr.s_addr<<endl;
	ret = listen(listenfd,10);
	assert(ret>=0);
	return listenfd;
}


int main(int argc,char* argv[]){
	const int event_num = 1000;
	const int max_num = 1000;
	epoll_event events[event_num];
	const char* ip = "210.45.118.68";
	int port = atoi(argv[1]);
	threadpool<Connection> pool(8,max_num);
	int listenfd = start_server(ip,port);
	//cout<<"listen fd "<<listenfd<<endl;
	cout<<"server start at "<<ip<<" "<<port<<endl;
	int epollfd = epoll_create(5);
	Connection::epollfd = epollfd;
	//cout<<"epollfd: "<<epollfd<<endl;
	add_fd(epollfd,listenfd,0);
	int accept_fd;
	std::vector<Connection> users(1000);
	//std::unordered_map<int,int> aux; // a map for fd with user
	while(true){
		int ready_num = epoll_wait(epollfd,events,event_num,-1);
		//cout<<"ready_num: "<<ready_num<<endl;
		if(ready_num<0 && errno!= EINTR){ 
			//the call was interrupted by a signal handler 
			cout<<"some error of epoll"<<endl;
			break;
		}
		for(int i=0;i<ready_num;++i){
			int cur_fd = events[i].data.fd;
			//cout<<"vec_len "<<users.size()<<endl;
			//cout<<"loop cur_fd"<<cur_fd<<endl;
			//cout<<"fd events "<<events[i].events<<endl;
			if(cur_fd==listenfd){
				struct sockaddr_in client_address;
				socklen_t len = sizeof(client_address);
				accept_fd = accept(listenfd,(struct sockaddr*)&client_address,&len);
				//cout<<client_address.sin_addr.s_addr<<endl;
				//cout<<"accept_fd: "<<accept_fd<<endl;
				if(Connection::user_count>=max_num){
					printf("too many people~\n");
					continue;
				}
				int vec_len = users.size();
				users[accept_fd].init(accept_fd,client_address);
				//cout<<"user comming"<<endl;
				//aux[accept_fd] = vec_len;
				//cout<<"vector len "<<vec_len<<endl;
			}
			else if(events[i].events&(EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
				//cout<<"close_connection"<<endl;
				users[cur_fd].close_connection();
			}
			else if(events[i].events & EPOLLIN){
				//cout<<"cur_fd "<<cur_fd<<endl;
				if(users[cur_fd].read()){
					//cout<<"read no problem"<<endl;
					pool.add_requests(&users[cur_fd]);
				}
				else{
					//cout<<"close_connection"<<endl;
					users[cur_fd].close_connection();
					//users.erase(users.begin()+aux[cur_fd]);
				}
			}
			else if(events[i].events & EPOLLOUT){
				//cout<<"haha not comming"<<endl;
				bool haha = users[cur_fd].write();
				//cout<<"haha "<<haha<<endl;
				if(!haha){
					//cout<<"I am closed~"<<endl;
					users[cur_fd].close_connection();
					//users.erase(users.begin()+aux[cur_fd]);
				}
			}
			else{
				//cout<<"hello"<<endl;
			}
		}
	}
	close(epollfd);
	close(listenfd);
	return 0;
}

