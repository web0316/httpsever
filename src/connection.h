#ifndef CONNECTION_H
#define CONNECTION_H
#include<unistd.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<string>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<vector>


class Connection{
public:
	static int epollfd;
	static int user_count;
	static const int filename_len;
	static const int read_buffer_len;
	static const int write_buffer_len;
	enum class Method {get,post,head};
	enum class Parse_state {parse_request_line,parse_header,parse_content};
	enum class Http_request {not_request,get_request,bad_request,
				forbidden_request,other_error,no_file_exit,file_request};
	enum class Line_status {line_ok,line_error,line_continue};
public:
	Connection(const std::string& hah="source");
	void init(int x_sockfd,const sockaddr_in& adddr);
	void close_connection();
	void process();
	bool read();
	bool write();
	~Connection() = default;
private:
	void init_aux();
	Http_request process_read();
	bool process_write(Http_request);
	Http_request parse_request_line(const std:: string&);
	Http_request parse_header(const std::string&);
	Http_request parse_content();
	Http_request deal_request();
	Line_status parse_line();
	void unmap();
	bool write_add_aux(const std::string& test);
	bool response_add(int sta,const std::string& test);
	bool content_add(const std::string& test);
	bool header_add(int);
	bool content_len_add(int);
	bool longconnect_add();
	void deal_iov(iovec*,int);
private:
	int sockfd;
	sockaddr_in client_address;
	std::string read_buffer;
	std::string write_buffer;
	int cur_read_idx;
	int cur_write_idx;
	int start_idx;
	Parse_state cur_state;
	Method cur_method;
	std::string request_file_name;
	//std::string default_file_name;
	std::string url;
	std::string version;
	std::string host;
	int content_len;
	bool keep_alive;
	char* real_file_address;
	struct stat file_stat;
	int judge;
	struct iovec iov[2];
	int iov_count;
    const std::string root_file;
	static const std::string default_file_name;
	static const std::string error404;
	static const std::string error400;
	static const std::string error403;
	static const std::string error500;
};
#endif
