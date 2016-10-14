#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define MYPORT "5000"
#define MAXNAMELEN 512

//using namespace std;

struct client_list
{
int cli_sock;//socket address for the client
struct sockaddr_in cli_server;
struct sockaddr_in cli_addr;//structure for storing the client address
int cli_file;//will store the file descriptor here
struct client_list* next;//pointer to the next client
};

unsigned short int getRandPort(){
    srand(time(NULL));
    unsigned short int range = rand()%2 + 2;
    srand(time(NULL));
    //unsigned short int port = (rand()%1000) + (range*10000);
    
    //unsigned short int port = rand()%16382 + 49152;
    unsigned short int port = rand()%100 + 5000;
    return port;
}

/*fetch data from file
**return value: 0--not reach end of file, -1--reach or beyond end of file;
**input: fd--file descriptor, *buf--buffer to store contect;
*/
int fetchData(int fd, char* buf) {
    int read_bytes;
    read_bytes = (int)read(fd, buf, 512);
    if (read_bytes == 512) {
        return 0;
    }
    else {
        return -1;
    }
}

/*use socket_fd to find client 
**if head, return head; else, return target's parent
*/
struct client_list* find(int socketfd, struct client_list *head)  
{
	if (socketfd == head->cli_sock)
		return head;
	else if (head->next == NULL)
		return NULL;
	else if (socketfd == head->next->cli_sock)
		return head;
	else 
		return(find(socketfd,head->next));
}

/*delete certain nodes
**return head;
**before using this function, use find() to find its parent
**if delete node is head, input itself
*/
struct client_list* delete_cli(struct client_list *head, struct client_list *del_parent)
{
	if (del_parent == head)
		return head->next;
	else
	{
		del_parent->next = del_parent->next->next;
		return head;
	}
}


int main (int argc, char * argv[])
{
    struct sockaddr_in server, client;
    struct addrinfo hints, *servinfo, *clientinfo,*p;
    socklen_t clientlen = sizeof(client);
    int sock, rbyte;
    char buffer[1024];
	char filedata[516];
    // char filebuf[65536];
    int fileptr;
    
    //latest joined client
    struct client_list* latest_cli=(struct client_list*)malloc(sizeof(struct client_list));
    //the very head of client list
    struct client_list* head=(struct client_list*)malloc(sizeof(struct client_list));
    struct client_list* tmp_cli=(struct client_list*)malloc(sizeof(struct client_list));
    struct client_list* tar_parent=(struct client_list*)malloc(sizeof(struct client_list));
    
    //supporting variables
    int i;
    int rv;
    int yes=1; 
    int newsock;
    char client_port[5];
    char ipstr[INET6_ADDRSTRLEN];
    char tmp_data[512];
    int fetch_res;
    int send_res;
    int blocknum;
    unsigned short int blockf;
    unsigned short int opcode1, opcode2; //opcode1-RRQ, opcode2-DATA
    
    //input arguments verify
    if(argc!=3)
    {
        printf("command line arguments incurrent: ./tftpserver <IP> <PORT>\n");
        return 1;
        
    }
    
    /*
    //sockadd_in initial
    memset(&server, 0, sizeof server);
    server.sin_addr.s_addr=inet_addr(argv[1]);
    server.sin_port=htons(atoi(argv[2]));
    server.sin_family=AF_INET; //ipv4
    
    //create sock
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Error in creating socket");
        return 2;
    }
    
    //bind sock
    if ((bind(sock, (struct sockaddr*)&server, sizeof(server))) == -1)
    {
        perror("Error in binding");
        return 3;
    }
    */
    
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}


	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));

		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
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
    //printf("listener: waiting to recvfrom...\n");
    
    printf("Server ready to receive request from Clients \n");

    //select fd_set variables
    fd_set master, reader;
    FD_ZERO(&master);
    FD_ZERO(&reader);
    int fdmax;
    fdmax = sock;
    FD_SET(sock, &master);
    
    while(1)
    {
        //printf("in while\n");
        //live check ready fds (no time-out limit)
        reader = master;
        //printf("in select\n");
        //rv = select(fdmax+1, &reader, NULL, NULL, NULL);
        
      
        //handle select error
        if (select(fdmax+1, &reader, NULL, NULL, NULL) == -1) {
            perror("Select ");
            return 4;
        }
        //printf("%d\n", fdmax);
        for (i = sock; i <= fdmax; i++) {
           
            //printf("i=%i\n", i);
            if (FD_ISSET(i, &reader)) {
                if (i == sock) { //add new client to list and its sock to master
                    printf("new connection come >>>\n");
                    rbyte = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client, &clientlen);
                    if (rbyte < 0)
                    {
                        perror("Couldn't receive data");
                        return 5;
                    }else {
                        printf("recv()'d %d bytes of data in buf ", rbyte);
                        printf("from IP address %s\n", inet_ntop(AF_INET, &client.sin_addr, ipstr, sizeof ipstr));
                    }
                    
                    
                    char filename[MAXNAMELEN];
                    memcpy(&opcode1,&buffer,2);
                    
                    //decode file name
                    int t;
        		    for (t=0; buffer[2+t]!='\0'; t++)
        		    {
        		        filename[t]=buffer[2+t];
        		    }	
        		    filename[t]='\0';
        		    printf("RRQ received, filename: %s\n", filename);
        		    
        		    //open file
        		    fileptr=open(filename,O_RDONLY);
        		    if (fileptr==-1)//file not found
        			{
        			    //prepare error packet and sent to client
            			char errstring[512]="file not founded";
            			unsigned short int errop=htons(5);
            			unsigned short int errcode=htons(1);
            			char errorbuffer[516];
            		    memcpy(&errorbuffer,&errop,2);
            			memcpy(&errorbuffer[2],&errcode,2);
            			memcpy(&errorbuffer[4],&errstring,512);
            			sendto(sock, errorbuffer, 516, 0, (struct sockaddr*)&client, sizeof(client));
        			}
        			else //file found, now create data packet
        			{
        			    
        			    bzero(tmp_cli,sizeof(*tmp_cli));
        			    tmp_cli->cli_file = fileptr;
            			//create ephemeral socket for new connections
            			unsigned short int rand_port = getRandPort();
            			printf("create new random port: %i\n", rand_port);
            			
            			/*
            			tmp_cli->cli_server.sin_addr.s_addr=server.sin_addr.s_addr;
                        tmp_cli->cli_server.sin_port=htons(rand_port);
                        tmp_cli->cli_server.sin_family=AF_INET; //ipv4
                        //create sock
                        newsock = socket(AF_INET, SOCK_DGRAM, 0);
                        if (newsock < 0)
                        {
                            perror("Error in creating new sock\n");
                        }
                        
                        
                        if (bind(newsock,(struct sockaddr *)&(tmp_cli->cli_server),sizeof(tmp_cli->cli_server)) < 0)
                        {
                             perror("Error in binding new sock\n");
                        }
                        */
                        
                        sprintf(client_port,"%d",rand_port);
                        if ((rv = getaddrinfo(NULL, client_port, &hints, &clientinfo)) != 0) {
									fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
									return 1;
								}


						// loop through all the results and bind to the first we can
						for(p = clientinfo; p != NULL; p = p->ai_next) {
							if ((newsock = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
								perror("listener: socket");
								continue;
							}

							setsockopt(newsock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));

							if (bind(newsock, p->ai_addr, p->ai_addrlen) == -1) {
								close(newsock);
								perror("newsock: bind");
								continue;
							}
							break;
						}	

						if (p == NULL) {
							fprintf(stderr, "newsock: failed to bind socket\n");
							return 2;
						}

						freeaddrinfo(clientinfo);

                        
                        //add sock and cli_addr to client list
                        tmp_cli->cli_sock = newsock;
                        memcpy(&tmp_cli->cli_addr, &client, sizeof(client));
                        printf("set new socket %d successfully\n", tmp_cli->cli_sock);
                        FD_SET(tmp_cli->cli_sock, &master);
                        if (tmp_cli->cli_sock > fdmax) {
                            fdmax = tmp_cli->cli_sock;
                        }
                        //set up head and latest_cli
                        if (head->cli_sock == 0) {
                            memcpy(head, tmp_cli, sizeof(head));
                            printf("first client come, set up head.\n");
                            memcpy(latest_cli, head, sizeof(head));
                        }
                        printf("head->cli_sock: %i\n",head->cli_sock);
                        
                        //handle next
                        memcpy(&latest_cli->next, &tmp_cli, sizeof(tmp_cli));
                        memcpy(&latest_cli, &tmp_cli,sizeof(tmp_cli));
                        //latest_cli->next = tmp_cli;
                        //latest_cli = tmp_cli; //refresh latest_cli 
                        
                        //create data packet
                        fetch_res = fetchData(fileptr, tmp_data);
                        
            			opcode2=htons(3);
            			blockf=htons(1); //block num
            			memcpy(&filedata[0],&opcode2,2);
            			memcpy(&filedata[2],&blockf,2);
            			memcpy(&filedata[4],&tmp_data,512);
            		
            		    
            		    send_res=sendto(tmp_cli->cli_sock,filedata,516,0,(struct sockaddr*)&(tmp_cli->cli_addr),sizeof(tmp_cli->cli_addr));
            			if (send_res < 0)
            			{
            			perror("couldn't send first packet: ");
                        }
                        else {
                            printf("send first block successfully.\n");
                        }
        			
                    }//is_set
                }
                else //old client
                {
                    printf("old client comes>>>\n");
                    //clear buffer and tmp_cli
                    bzero(buffer, sizeof(buffer));
                    bzero(filedata, sizeof(filedata));
                    bzero(tmp_data, sizeof(tmp_data));
                    bzero(tmp_cli,sizeof(tmp_cli));
                    //bzero(client,sizeof(&client));
                    //find this old client
                    tar_parent = find(i, head);
                    //printf("1\n");
                    //head or successors
                    //printf("head->cli_sock: %i\n",head->cli_sock);
                    //printf("tar_parent->cli_sock: %i\n", tar_parent->cli_sock);
                    if (tar_parent->cli_sock == head->cli_sock) {
                        memcpy(tmp_cli, tar_parent, sizeof(tar_parent));
                         //printf("2\n");
                    }
                    else {
                        memcpy(tmp_cli, tar_parent->next, sizeof(tar_parent));
                         //printf("3\n");
                    }
                    
                    
                    rbyte = recvfrom(tmp_cli->cli_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&tmp_cli->cli_addr, &clientlen);
                     //printf("4\n");
                    tmp_cli->cli_addr = client;
                    if (rbyte < 0)
                    {
                        perror("Couldn't receive data\n");
                        return 6;
                    }else {
                        printf("recv()'d %d bytes of data in buf ", rbyte);
                        printf("from IP address %s\n", inet_ntop(AF_INET, &tmp_cli->cli_addr.sin_addr, ipstr, sizeof ipstr));
                    }
                    memcpy(&opcode1, &buffer[0], 2);
                    if (ntohs(opcode1) == 4)
                    {
                        
                        bzero(&blocknum, sizeof(blocknum));
                        memcpy(&blocknum, &buffer[2], 2);
                        blocknum = ntohs(blocknum);
                        printf("ACK %i received\n", blocknum);
                        //check reach end of file
                        fetch_res = fetchData(tmp_cli->cli_file, tmp_data);
                        if (fetch_res == 0) {
                            blockf = htons(blocknum+1);
                            opcode2 = htons(3);
                            memcpy(&filedata[0], &opcode2, 2);
                            memcpy(&filedata[2], &blockf, 2);
                            memcpy(&filedata[4], tmp_data, 512);
                            
                            
                            int send_res=sendto(tmp_cli->cli_sock, filedata, 516, 0, (struct sockaddr*)&(tmp_cli->cli_addr), clientlen);
    						if (send_res < 0) {
    						    perror("Couldn't send packet: ");
    						}
                        }
                        else { //reach end of file, close sock and delete client and remove form master
                            printf("full file is sent and connection is closed.\n");
                            close(tmp_cli->cli_sock);
                            close(tmp_cli->cli_file);
                            //head = delete_cli(head, tar_parent);
                            FD_CLR(tmp_cli->cli_sock, &master);
                        }
                    }
                }  
            }
        }
    }
    
    return 0;
}
 