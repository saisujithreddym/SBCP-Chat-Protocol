/*
** client.c -- a client part of chat room service
*/
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 800

/*
** chat message struct
*/
struct sbcp_attribute               
    {                                   
	int16_t type;            // Attribute type:1,reason 2,username 3,client count 4,message        
	int16_t length;          // Attribute length
	char*  communicate_data; // Attribute message data
    };
struct sbcp_message               
    {                                   
	int8_t vrsn;             // message type version: 3
	int8_t type;             // message type:2,join 3,fwd 4,send
	int16_t length;          // message length    
	struct sbcp_attribute* attr_data;   // message attribute              
    };
 
/*
** packi16() -- store a 16-bit int into a char buffer (like htons())
*/
void packi16(char *buf, unsigned int i)
{
*buf++ = i>>8; *buf++ = i;
}
/*
** unpacki16() -- unpack a 16-bit int from a char buffer (like ntohs())
*/
unsigned int unpacki16(char *buf)
{
return (buf[0]<<8) | buf[1];
}
/*
** pack() -- store data dictated by the format string in the buffer
**
** h - 16-bit
** c - 8-bit char
** s - string (16-bit length is automatically prepended)
*/
int32_t pack(char *buf, char *format, ...)
{
    va_list ap;
    int16_t h;
	int8_t c;
    char *s;
    int32_t size = 0, len;
    va_start(ap, format);
    for(; *format != '\0'; format++) {
		switch(*format) {
			case 'h': // 16-bit
				size += 2;
				h = (int16_t)va_arg(ap, int); // promoted
				packi16(buf, h);
				buf += 2;
				break;
			case 'c': // 8-bit
				size += 1;
				c = (int8_t)va_arg(ap, int); // promoted
				*buf++ = (c>>0)&0xff;
				break;
			case 's': // string
				s = va_arg(ap, char*);
				len = strlen(s);
				size += len + 2;
				packi16(buf, len);
				buf += 2;
				memcpy(buf, s, len);
				buf += len;
				break;
		}
	}
	va_end(ap);
	return size;
}
/*
** unpack() -- unpack data dictated by the format string into the buffer
*/
void unpack(char *buf, char *format, ...)
{
	va_list ap;
	int16_t *h;
	int8_t *c;
	char *s;
	int32_t len, count, maxstrlen=0;
	va_start(ap, format);
	for(; *format != '\0'; format++) {
		switch(*format) {
            case 'h': // 16-bit
				h = va_arg(ap, int16_t*);
				*h = unpacki16(buf);
				buf += 2;
				break;
			case 'c': // 8-bit
				c = va_arg(ap, int8_t*);
				*c = *buf++;
				break;
			case 's': // string
				s = va_arg(ap, char*);
				len = unpacki16(buf);
				buf += 2;
				if (maxstrlen > 0 && len > maxstrlen) count = maxstrlen - 1;
				else count = len;
				memcpy(s, buf, count);
				s[count] = '\0';
				buf += len;
				break;
			default:
				if (isdigit(*format)) { // track max str len
					maxstrlen = maxstrlen * 10 + (*format-'0');
				}
		}
		if (!isdigit(*format)) maxstrlen = 0;
	}
	va_end(ap);
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	
	
    int sockfd, numbytes;
	int16_t packetsize;
	int rv,len,i;
	struct addrinfo hints, *servinfo, *p;    
    struct sbcp_message msg_send;
	struct sbcp_attribute  attr_send;
	struct sbcp_message;
	struct sbcp_attribute;
	char buf[MAXDATASIZE];
	char buf_username_send[16];
	char buf_message_send[512];
	fd_set read_fds;          // temp file descriptor list for select()
    FD_ZERO(&read_fds);       // initialize the set
		
    if (argc != 4) {
        fprintf(stderr,"usage: client username server_ip server_port \n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }


    freeaddrinfo(servinfo); // all done with this structure
    
    // prepare data of username
    strcpy(buf_username_send,argv[1]);
	attr_send.communicate_data=buf_username_send;
	attr_send.type=2;
    attr_send.length=20;
    msg_send.vrsn='3';
	msg_send.type='2';
	msg_send.length=24;
	msg_send.attr_data=&attr_send;
    // chage data to network byte order
	packetsize = pack(buf,"cchhhs", msg_send.vrsn, msg_send.type, msg_send.length, attr_send.type,attr_send.length, buf_username_send);
	// send data of username
    if (send(sockfd, buf, packetsize, 0) == -1){
						perror("send");
						exit(1);
					}

	// prepare data of chat message
	attr_send.communicate_data=buf_message_send;
	attr_send.type=4;
    attr_send.length=516;
    msg_send.vrsn='3';
	msg_send.type='4';
	msg_send.length=520;
	msg_send.attr_data=&attr_send;

	FD_SET(0, &read_fds); // add the keyboard input to the read_fds set
	FD_SET(sockfd, &read_fds);// add the sockfd to the read_fds set

	while(1) {
		
		if (select(sockfd+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		for(i = 0; i <= sockfd; i++) {// find if there is data to read from server or keyboard
			
			
			if (FD_ISSET(i, &read_fds)) {
				
				if (i == 0) {//there is data to read from keyboard
						fgets(buf_message_send, sizeof(buf_message_send), stdin);
	                    len = strlen(buf_message_send) - 1;
	                    if (buf_message_send[len] == '\n')
	                    buf_message_send[len] = '\0';
						// chage data to network byte order
						packetsize = pack(buf,"cchhhs", msg_send.vrsn, msg_send.type, msg_send.length,
						attr_send.type,attr_send.length, buf_message_send);
						// send chat message to server
					if (send(sockfd, buf, packetsize, 0) == -1) {
						perror("send");
						exit(1);
					}
				}
				
				if (i == sockfd){//there is data to read from server
					// receive chat message to server
					if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) <= 0) {
						//perror("recv");
						exit(1);
					}
					buf[numbytes] = '\0';
					printf("%s\n",buf);
				}
				
			}
			
			FD_SET(0, &read_fds);
	        FD_SET(sockfd, &read_fds);
		}
	}
	close(sockfd);

    return 0;
}
