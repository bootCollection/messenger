#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MYPORT "5000"	// the port users will be connecting to

#define MAXBUFLEN 100

char* message = "A";
bool escape = true;
int state = 0;
bool reset = 0;
std::vector<std::string> stor;
std::vector<std::time_t> stortime;
// a thread can wait for a specified amount of time on this condition variable and the thread can also be waken up before the timer expires
std::condition_variable cv_timer;  
// protect concurrent access by the other thread; 
std::mutex exclude_other_mtx;  

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int inPort2() {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
	//	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);
	
	while(escape) {
	//	printf("listener: waiting to recvfrom...\n");

		addr_len = sizeof their_addr;
		if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		//	perror("recvfrom");
		//	exit(1);
		}

	//	printf("listener: got packet from %s\n",
	//		inet_ntop(their_addr.ss_family,
	//			get_in_addr((struct sockaddr *)&their_addr),
	//			s, sizeof s));
	//	printf("listener: packet is %d bytes long\n", numbytes);
		buf[numbytes] = '\0';
	//	printf("listener: packet contains \"%s\"\n", buf);
		message = &buf[0];
		if(std::string(buf) == "/0") {
			state = 0;
		} else if(std::string(buf) != "")  {
		reset = 1;
		state = 2;
		stor.push_back(buf);
		stortime.push_back(std::time(0));
		memset(buf, 0, MAXBUFLEN);
		}
	}	
	close(sockfd);
	return 0;
}
int outPort(int argc, char *argv[])
{
	struct addrinfo hints, *res, *p;
	int status;
	char ipstr[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: showip hostname\n");
	    return 1;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_DGRAM;
	

	if ((status = getaddrinfo(argv[1], "5000", &hints, &res)) != 0) {
//		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 2;
	}

//	printf("IP addresses for %s:\n\n", argv[1]);

	for(p = res;p != NULL; p = p->ai_next) {
		void *addr;
		char *ipver;

		// get the pointer to the address itself,
		// different fields in IPv4 and IPv6:
		if (p->ai_family == AF_INET) { // IPv4
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
		} else { // IPv6
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			ipver = "IPv6";		
		}

		// convert the IP to a string and print it:
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
		printf("[Connected to :: %s: %s]\n", ipver, ipstr);
	}
	const char *msg;
	int len;
	len = strlen(msg);
	int s;
	int r;

	s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);	
	bind(s, res->ai_addr, res->ai_addrlen);
	while(escape) {
		std::string sMsg;
		std::cout << "Enter message: ";
		std::getline(std::cin, sMsg);
		msg = sMsg.c_str();
		len  = strlen(msg);
		if(sMsg[0] == '/') {
			if(sMsg == "/end") {
				escape = false;
				std::cout << "~~Quiting~~" << std::endl;
				//The terminate here causes a messy quit
				msg = "/0";
				r = sendto(s, msg, len, 0, res->ai_addr, res->ai_addrlen);
				//std::terminate();
				break;
			} else if(sMsg == "/state") {
				std::cout << "State::";
				if(state == 0)
					std::cout << "Disconnected" << std::endl;
				if(state == 1)
					std::cout << "Away" << std::endl;
				if(state == 2)
					std::cout << "Online" << std::endl;
			} else {
			std::cout << "!Command not recognized!" << std::endl;
			}
		} else {
		r = sendto(s, msg, len, 0, res->ai_addr, res->ai_addrlen);
		if(stor.size() > 0) {
			std::cout << "[" << stor.size() << "] New Messages" << std::endl;
			for(int i = 0; i < stor.size(); i++) {
				std::cout << "[" << i + 1 << "] " << ctime(&stortime[i]) << "  |: ";
				std::cout << stor[i] << std::endl;
			}
		}
		stor.clear();
		}
	}
	close(s);

	freeaddrinfo(res); // free the linked list
	
	return 0;
}

void sleeper() {
	while(escape) {
	for(int i = 0; i < 6 && escape; i++) {
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
		if(reset) {
			reset = 0;
		} else if(state != 0) {
		state = 1;
		}
	}
}

int main(int argc, char *argv[])  {
  std::thread tinPort(inPort2);
  std::thread tSleeper(sleeper);
  outPort(argc, argv);
  tinPort.join();
  tSleeper.join();
  return 0;
}
