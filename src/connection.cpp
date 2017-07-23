#include"connection.h"
#include<iostream>
#include<string>
#include<memory>
using std::string;
using std::vector;
using std::cout;
using std::endl;
//static member 

//some aux functions
void set_fd_nonblocked(int fd){
	int old_prioty = fcntl(fd,F_GETFL);
	int new_prioty = old_prioty | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_prioty);
}

void add_fd(int epollfd,int fd,bool keep_alive){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if(keep_alive)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	set_fd_nonblocked(fd);
}

void remove_fd(int epollfd,int fd){
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}

void add_fd_property(int epollfd,int fd,int added){
	epoll_event event;
	event.data.fd = fd;
	event.events = added | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void parse_string(const string& test,vector<string>& res){
	int pos = -1;
	int cur;
	//cout<<test<<endl;
	while(1){
		cur = test.find(" ",pos+1);
		//cout<<cur<<endl;
		if(cur==-1){
			res.push_back(test.substr(pos+1));
			return;
		}
		res.push_back(test.substr(pos+1,cur-pos-1));
		pos = cur;
	}
}

//the member function of class
const int Connection::filename_len = 256;
const int Connection::read_buffer_len = 2048;
const int Connection::write_buffer_len = 20480;



const string Connection::error404 = "The request resource could not be found but may be available in the future.";
const string Connection::error400 = "The server cannot or will not process the request due to an appanrent client error.";
const string Connection::error403 = "The request was valid, but the server is refusing the action.";
const string Connection::error500 = "A generivc error message,an expected condition was encountered and no more specific message.";
const string Connection::default_file_name = "source/index.html";


int Connection::user_count = 1;
int Connection::epollfd = -1;

//the construct function
Connection::Connection(const string& haha):root_file(haha){}
void Connection::init(int x_sockfd,const sockaddr_in& addr){
	    sockfd = x_sockfd;
		client_address = addr;
		add_fd(epollfd,sockfd,true);
		//cout<<"epollfd: "<<epollfd<<endl;
		//cout<<"epollfd "<<epollfd<<endl;
		//cout<<"sockfd "<<sockfd<<endl;
		init_aux();
		//cout<<"added finished"<<endl;
	}

//other member function
void Connection::init_aux(){
	cur_read_idx = 0;
	cur_write_idx = 0;
	start_idx = 0;
	cur_state = Parse_state::parse_request_line;
	cur_method = Method::get;
	content_len = 0;
	keep_alive = 0;
	read_buffer.clear();
	write_buffer.clear();
	request_file_name.clear();
}


bool Connection::read(){
	//cout<<"buffer_sz "<<read_buffer.size()<<endl;
	if(read_buffer.size() > read_buffer_len)
		return false;
	int read_cnt;
	int cur_read = 0;
	//cout<<"some error before "<<read_buffer_len<<endl;
	//std::shared_ptr<char> tem(new char[read_buffer_len],std::default_delete<char[]>());
	char* tem = new char[read_buffer_len];
	//cout<<"some error after?"<<endl;
	while(1){
		//cout<<"read no comming"<<endl;
		//read_cnt = recv(sockfd,tem.get()+cur_read,read_buffer_len-cur_read,0);
		read_cnt = recv(sockfd,tem+cur_read,read_buffer_len-cur_read,0);
		//cout<<"tem no error"<<endl;
		if(!read_cnt)
			return false;
		if(read_cnt==-1){
			if(errno==EAGAIN || errno == EWOULDBLOCK)	
				break;
			return false;
		}
		cur_read+=read_cnt;
	}
	//cout<<"cur noerror"<<endl;
	//string tot(tem.get(),tem.get()+cur_read);
	string tot(tem,tem+cur_read);
	delete [] tem;
	read_buffer += tot;
	//cout<<read_buffer<<endl;
	return true;
}

Connection::Line_status Connection::parse_line(){
	int len = read_buffer.size();
	//cout<<"parse_line: "<<endl;
	for(;cur_read_idx<len;++cur_read_idx){
		char tem = read_buffer[cur_read_idx];
		if(tem=='\r'){
			if(cur_read_idx+1==len){
				//cout<<"error continue"<<endl;
				return Line_status::line_continue;
			}
			else if (read_buffer[cur_read_idx+1]=='\n'){
				++cur_read_idx;
			//	cout<<"line__ok"<<endl;
				return Line_status::line_ok;
			}
		}
		else if(tem=='\n'){
			if(cur_read_idx>=1&&read_buffer[cur_read_idx-1]=='\r'){
				//cout<<"link__ok"<<endl;
				return Line_status::line_ok;
			}
		}
	}
	//cout<<"error occured"<<endl;
	return Line_status::line_error;
}

Connection::Http_request Connection::parse_request_line(const string& tem){
	vector<string> res;
	parse_string(tem,res);
	int len = res.size();
	//cout<<"res len"<<len<<endl;
	//for(int i=0;i<len;++i)
		//cout<<res[i]<<endl;
	if(tem.size()==0)
		return Http_request::bad_request;
	//int len = res.size();
	//cout<<"res[0] size"<<res[0].size()<<endl;
	//cout<<"res[len-1] size"<<res[len-1].size()<<endl;
	//cout<<"res value "<<res[0]<<" "<<res[len-1]<<endl;
	if(res[0]!="GET"|| res[len-1]!="HTTP/1.1"){
		//cout<<"request_line_error"<<endl;
		return Http_request::bad_request;
	}
	if(len==3)
		url = res[1];
	//for(int i=0;i<len;++i)
		//cout<<res[i]<<" ";
	cur_state = Parse_state::parse_header;
	//cout<<"request_line cur_state"<<(int)cur_state<<endl;
	return Http_request::not_request;
}

Connection::Http_request Connection::parse_header(const string& tem){
	vector<string> res;
	parse_string(tem,res);
	if(res.size()<2){
		//cout<<"come here"<<endl;
		if(content_len){
			cur_state = Parse_state::parse_content;
			return Http_request::not_request;
		}
		return Http_request::get_request;
	}
	string tot = res[0];
	//cout<<"header "<<tot<<endl;
	if(tot=="Host:")
		host = res[1];
	else if(tot=="Connection:"){
		if(res[1]=="keep-alive")
			keep_alive = 1;
	}
	else if(tot=="Content_Length:")
		content_len = stoi(res[1]);
	return Http_request::not_request;
}

Connection::Http_request Connection::parse_content(){
	if(read_buffer.size() >= cur_read_idx+content_len)
		return Http_request::get_request;
	return Http_request::not_request;
}

Connection::Http_request Connection::process_read(){
	//cout<<"welcom to process_read"<<endl;
	Line_status cur_line = Line_status::line_ok;
	Http_request cur_http = Http_request::not_request;
    while( ((cur_state == Parse_state::parse_content) && (cur_line == Line_status::line_ok)) 
			|| ((cur_line = parse_line())==Line_status::line_ok)){
		//cout<<"read_buffer size "<<read_buffer.size()<<endl;
		//cout<<"start_idx "<<start_idx<<endl;
		//cout<<"cur_read_idx "<<cur_read_idx<<endl;
		cur_read_idx++;
		string tem = read_buffer.substr(start_idx,cur_read_idx-start_idx-2);
		start_idx = cur_read_idx;
		//cout<<"one header "<<tem<<endl;
		switch(cur_state){
			case Parse_state::parse_request_line:{
				//cout<<"parse_request_line OK"<<endl;
				cur_http = parse_request_line(tem);
				//cout<<"enum value: "<<(int)cur_http<<endl;
				//cout<<"cur_state: "<<(int)cur_state<<endl;
				if(cur_http==Http_request::bad_request)
					return Http_request::bad_request;
				break;
			}
			case Parse_state::parse_header:{
				cur_http = parse_header(tem);
				if(cur_http==Http_request::get_request)
					return deal_request();
				break;
			}
			case Parse_state::parse_content:{
				cur_http = parse_content();
				if(cur_http==Http_request::get_request)
					return deal_request();
				cur_line = Line_status::line_continue;
				break;
			}
			default:{
				return Http_request::other_error;
			}
		}
	}
}


Connection::Http_request Connection::deal_request(){
	//cout<<"hello welcom to deal_request"<<endl;
	//cout<<"url "<<url<<endl;
	if(url=="/"){
		//cout<<"no url"<<endl;
		request_file_name = default_file_name;
		//return Http_request::file_request;
	}
	else
		request_file_name = root_file + url;
	/*std::shared_ptr<char> tem_char(new char[request_file_name.size()],std::default_delete<char[]>());
	strcpy(tem_char.get(),request_file_name.c_str());*/

	//char* tem_char = new char[request_file_name.size()];
	if(stat(request_file_name.c_str(),&file_stat)<0){
		//cout<<"no file"<<endl;
		//delete [] tem_char;
		return Http_request::no_file_exit;
	}
    if(!(file_stat.st_mode & S_IROTH)){
		//delete [] tem_char;
		return Http_request::forbidden_request;
	}
	if(S_ISDIR(file_stat.st_mode)){
		//delete [] tem_char;
		return Http_request::bad_request;
	}
	
	int fd = open(request_file_name.c_str(),O_RDONLY);
	real_file_address = (char*)mmap(0,file_stat.st_size,
			PROT_READ,MAP_PRIVATE,fd,0);
	//delete [] tem_char;
	return Http_request::file_request;
}
void Connection::unmap(){
	if(real_file_address)
		munmap(real_file_address,file_stat.st_size);
}

bool Connection::write_add_aux(const string& test){
	//cout<<"write_buffer size"<<write_buffer.size()<<endl;
	if(write_buffer.size()+test.size()>write_buffer_len)
		return false;
	write_buffer += test;
	return true;
}

bool Connection::response_add(int tem,const string& test){
	string tot;
	tot = "HTTP/1.1 " + std::to_string(tem)+" "+ test + "\r\n";
	return write_add_aux(tot);
}

bool Connection::content_len_add(int content_len){
	string tem;
	tem = "Content-Length: "+ std::to_string(content_len)+"\r\n";
	return write_add_aux(tem);
}

bool Connection::header_add(int content_len){
	content_len_add(content_len);
	longconnect_add();
	return write_add_aux("\r\n");
}

bool Connection::longconnect_add(){
	string tem;
	if(keep_alive)
		tem="keep_alive";
	else tem = "close";
	string tot;
	tot = "Connection: " + tem + "\r\n";
	return write_add_aux(tot);
}


bool Connection::process_write(Connection::Http_request test){
	switch(test){
		case Http_request::forbidden_request:{
			response_add(403,"Forbidden");
			header_add(error403.size());
			if(!write_add_aux(error403))
				return false;
			break;
		}
		case Http_request::bad_request:{
			response_add(400,"Bad Request");
			header_add(error400.size());
			if(!write_add_aux(error400))
				return false;
			break;
		}
		case Http_request::no_file_exit:{
			//cout<<"before   "<<write_buffer<<endl;
			//cout<<"hello 404"<<endl;
			response_add(404,"Not Found");
			header_add(error404.size());
			if(!write_add_aux(error404))
				return false;
			//cout<<"after "<<write_buffer<<endl;
			break;
		}
		case Http_request::other_error:{
			cout<<"welcome to 500"<<endl;
			response_add(500,"Internal Server Error");
			header_add(error500.size());
			if(!write_add_aux(error500))
				return false;
			break;
		}
		case Http_request::file_request:{
			response_add(200,"OK");
			header_add(file_stat.st_size);
			iov[1].iov_base = real_file_address;
			iov[1].iov_len = file_stat.st_size;
			iov_count = 2;
			judge = 1;                //check whether there is a file
			return true;
		}
		default:{
			return false;
		}
	}
	iov_count = 1;
	judge = 0;
	return true;
}

void Connection::process(){
	auto res = process_read();
	if(res==Http_request::not_request){
		add_fd_property(epollfd,sockfd,EPOLLIN);
		return;
	}
	//cout<<"the value of res: "<<(int)res<<endl;
	bool tem = process_write(res);
	//cout<<"the value of tem "<<tem<<endl;
	if(!tem)
		close_connection();
	add_fd_property(epollfd,sockfd,EPOLLOUT);
}

void Connection::deal_iov(struct iovec* iov,int len){
	char* tem = (char*)iov[0].iov_base + len;
	if(len <= iov[0].iov_len){
		iov[0].iov_base = tem;
		iov[0].iov_len -= len;
	}
	else{
        tem = (char*)iov[1].iov_base + len;
		len -= iov[0].iov_len;
		iov[0].iov_len = 0;
		iov[1].iov_len -= len;
		iov[1].iov_base =  tem;
	}
}



bool Connection::write(){
	//cout<<"welcome to write"<<endl;
	int len = write_buffer.size();
	//cout<<"len "<<len<<endl;
	if(!len){
		add_fd_property(epollfd,sockfd,EPOLLIN);
		init_aux();
		//delete [] tem;
		return true;
	}
	//std::shared_ptr<char> tem(new char[len],std::default_delete<char[]>());
	//strcpy(tem.get(),write_buffer.c_str());
	char* tem = new char[len+1];
	strcpy(tem,write_buffer.c_str());
    iov[0].iov_base = tem;
	iov[0].iov_len = len;
    int total_len = iov[0].iov_len;
	//cout<<"judge "<<judge<<endl;
    if(judge)
		total_len += iov[1].iov_len;
	//cout<<"write_buffer: "<<write_buffer<<endl;
	//cout<<"total_len "<<total_len<<endl;
	//cout<<"buffer_sz"<<write_buffer.size()<<endl;

    int send_tem = 0;
	while(1){
		send_tem = writev(sockfd,iov,iov_count);
		//cout<<"send_tem"<<send_tem<<endl;
		if(send_tem<0){
			delete[] tem;
			if(errno==EAGAIN){
				add_fd_property(epollfd,sockfd,EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}
		total_len -= send_tem;
		if(!total_len){
			delete [] tem;
			unmap();
			//cout<<"total_len "<<total_len<<endl;
			if(keep_alive){
				init_aux();
				add_fd_property(epollfd,sockfd,EPOLLIN);
				return true;
			}
	        else{
				//cout<<"not keep_alive"<<endl;
				add_fd_property(epollfd,sockfd,EPOLLIN);
				return false;
			}
		}
		else deal_iov(iov,send_tem);
	}
}



void Connection::close_connection(){
	if(sockfd!=-1){
		remove_fd(epollfd,sockfd);
		sockfd = -1;
		--user_count;
	}
}
