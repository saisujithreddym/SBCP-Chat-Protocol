/*
** server.c -- a server part of chat room service
*/
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


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
int main(int argc,char *argv[])
{
	fd_set master; 
    fd_set read_fds; 
    int fdmax; 
    int listener; 
    int newfd; 
    struct sockaddr_storage remoteaddr; // client address 
    socklen_t addrlen;
    char buf[800]; 
	char buf_message_send[550];
	char buf_username_recv[16];
	char buf_message_recv[512];
	char names[100][16];
	char names_info[200];
	char exit_info[50];
	struct sbcp_message msg_recv;
	struct sbcp_attribute  attr_recv;
    int nbytes;
    int yes=1; 
    int i, j, rv,total_count=0;
    struct addrinfo hints, *ai, *p;

	if (argc != 4) {
        fprintf(stderr,"usage: server server_ip server_port max_clients\n");
        exit(1);
    }
	int countlimit=atoi(argv[3]); //maximum clients can enter the chat room
	
    FD_ZERO(&master); // initialize the set
    FD_ZERO(&read_fds);
    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
   
    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &ai)) != 0) {
    fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
    exit(1);
	}
	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;
		}
		
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}
		break;
	}
	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}
	freeaddrinfo(ai); // all done with this
	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}
	// add the listener to the master set
	FD_SET(listener, &master);
	// find the biggest file descriptor
	fdmax = listener; 
	
	// main loop
	for(;;) {
		read_fds = master; 
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		// find if there is data to read from existing connections
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // there is data to read from existing connections
				if (i == listener) {// there is a client asking to connect
					if(total_count<countlimit) // if the total client number small then the maximum
					{
						addrlen = sizeof remoteaddr;
						newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);// accept connection
						if (newfd == -1) {
							perror("accept");
						} else {
							FD_SET(newfd, &master); // add to master set
							if (newfd > fdmax) { // find the max
								fdmax = newfd;
							}
							total_count++;// record how many clients connected
						}
					}
				} else {// connected client send sth
					if ((nbytes = recv(i, buf, 800, 0)) <= 0) {// got error or connection closed by client
						if (nbytes == 0) {// connection closed
							sprintf(exit_info,"client %s has exited",names[i]);
							names[i][0]='\0';// reset name to be reusable

							// inform other clients that a client has exited
							for(j = 0; j <= fdmax; j++){
								if (FD_ISSET(j, &master)){
									if (j != listener && j != i){ // except the listener and the exited client
										if (send(j, exit_info, sizeof exit_info, 0) == -1){
											perror("send");
										}
									}
								}
							}
						} else {
							perror("recv");
						}
						total_count--;
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					} else {
						//change data from network byte order to normal
						unpack(buf, "cchhh", &msg_recv.vrsn, &msg_recv.type, &msg_recv.length, 
							&attr_recv.type,&attr_recv.length);
						if(msg_recv.type=='2'&&attr_recv.type==2){//received username
					    
							unpack(buf+8, "s", buf_username_recv);
							int j,b,k=1;
							for(j=4;j<=fdmax;j++){// as initial socket is always 4
								if(strcmp(buf_username_recv,names[j])==0){//username already used by former clients
									k=0;//identifier that the username cannot be used
									if (send(i, "choose another user name, forced out!", 50, 0) == -1){
										perror("send");}
									total_count--;
									close(i); // bye!
									FD_CLR(i, &master); // remove from master set
									break;
								}
							}
							if (k==1){//username is usable
								//successfully enter the room, server send all other clients username to the client
								sprintf(names[i],"%s",buf_username_recv);
								strcpy (names_info,"names of clients in the chat room: ");
								for(b=4;b<=fdmax;b++){
									strcat (names_info,names[b]);
								    strcat (names_info," ");
								}
								if (send(i,names_info , 200, 0) == -1){
									perror("send");
								}
							}
						}
						if(msg_recv.type=='4'&&attr_recv.type==4)//received chat message
						{
					    //change data from network byte order to normal
						unpack(buf+8, "s", buf_message_recv);
						sprintf(buf_message_send, "<%s>:%s",names[i],buf_message_recv);
						for(j = 0; j <= fdmax; j++) 
						{
							if (FD_ISSET(j, &master)) 
							{
								// except the listener and client
								if (j != listener && j != i) 
								{
									
									if (send(j, buf_message_send, nbytes, 0) == -1)
									{
										perror("send");
									}
								}
							}
						}
						}						
						
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
	return 0;
}
