#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif // __linux
#ifdef WIN32
#include <ws2tcpip.h>
#endif // WIN32
#include <thread>
#include <mutex>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
	printf("syntax : echo-server <port> [-e[-b]]\n");
	printf("sample : echo-server 1234 -e -b\n");
}

std::vector<int> clients;
std::mutex clients_mutex;

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				i++;
				continue;
			} // -e -> echo 옵션

			if (strcmp(argv[i], "-b") == 0) {
				broadcast = true;
				i++;
				continue;
			} // -b -> echo 옵션

			if (i < argc) port = atoi(argv[i++]); // port 정수로
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) { // 연결 종료 or 에러
			fprintf(stderr, "recv return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			ssize_t res_s = ::send(sd, buf, res, 0);
			if (res_s == 0 || res_s == -1) {
				fprintf(stderr, "send return %zd", res_s);
				myerror(" ");
				break;
			}
		}
		if (param.broadcast) {
			std::lock_guard<std::mutex> lock(clients_mutex);
			for(size_t i=0; i < clients.size(); ++i){
				if(clients[i] != sd){
					res = ::send(clients[i], buf, res, 0);
				} 
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %zd", res);
					myerror(" ");
					break;
				}
			}

		}
	}
	printf("disconnected\n");
	fflush(stdout);

	{
		std::lock_guard<std::mutex> lock(clients_mutex);
		for(size_t i=0; i < clients.size(); ++i){
			if(clients[i] == sd){
				clients.erase(clients.begin() + i);
				break;
			}
		}
        
    }

	::close(sd);
}

#ifdef WIN32
// declared in w2ipdef.h
#define TCP_KEEPALIVE       	 3
#define TCP_KEEPCNT              16
#define TCP_KEEPIDLE             TCP_KEEPALIVE
#define TCP_KEEPINTVL            17
#endif // WIN32

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) { 
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	//
	// socket -> 통신용 소켓
	//
	int sd = ::socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

#ifdef __linux__
	//
	// setsockopt
	//
	{
		int optval = 1;
		int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
		if (res == -1) {
			myerror("setsockopt");
			return -1;
		}
	}
#endif // __linux

	//
	// bind -> 서버 IP/PORT 지정
	//
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = param.srcIp; // 0 -> INADDR_ANY
		addr.sin_port = htons(param.port);

		ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	//
	// listen -> 연결 요청 대기
	//
	{
		int res = listen(sd, 5);
		if (res == -1) {
			myerror("listen");
			return -1;
		}
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len); // -> 실제 통신 소켓
		if (newsd == -1) {
			myerror("accept");
			break;
		}

		{
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(newsd); 
        }
		
		std::thread* t = new std::thread(recvThread, newsd);
		t->detach();
	}
	::close(sd);
}
