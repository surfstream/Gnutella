

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h> 
#include <strings.h>
#include <sys/stat.h>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <string>
#include <netdb.h>

using namespace std;
/*
 *  ------------Variables updated from the g04.conf file--------------
 *	int neighbor_port - port at which server listens for neighbor connections
 * 	int file_port - port at which server listens for file transfer connections
 * 	int number_peers - number of peers which the client needs to connect
 * 	TTL - Time To Live for the any packet sent by the client
 * 	is_seed - whether the current node is seed not or not
 * 	host_cache - the hostcache file
 * 	backup_cache - the backhostcache file
 * 	seed_nodes - file containing list of seed_nodes
 * 	local_files - file containing local files with the tags
 * 	local_file_directory - file containing local directory where the downloaded items shall be kept
 * --------------------------------------------------------------------*/

struct message {
	int mid[4];
	int	pid;
	int ttl;
	int hops;
	int plength;
	char payload[1000];
};

struct result
{
	int index;
	char filename[100];
	char filesize[30];
	char location[20];	
};

struct result store[50];

struct argument{
	int index;
	char fl[100];
	int len;
};


int mypings[200],otherpings[200][2],ping_counter=0,other_counter=0;

int indexcounter=1;

int myqueries[200],querycounter=0;; //index 0 for storing the mid and 1 for storing the qurey hit counters for that query
int otherqueries[200][2],other_query_counter=0;
char input[512]; 							//for getting user input

char connect_msg[] = "GNUTELLA CONNECT/0.4\r\n";
char ok_msg[] = "GNUTELLA OK\r\n";

string host_cache, backup_cache ,seed_nodes, local_files, local_file_directory ;

int neighbor_port , file_port, number_peers,TTL, is_seed;

char pongIPstore[20][50];

int pongPORTstore[20],pongcounter=0;

int sockfd_clt[20];

int sockfd_serv_neigh, sockfd_serv_file;

socklen_t sock_neigh_len , sock_file_len ;

char* searchfiles[50][20];

fd_set allset, rset;

int neigh_count=0; //number of neighbours currently connected.

const unsigned MAXBUFLEN = 512;

const unsigned MID_size = 16;			//message ID size limit 16 bytes
string prompt = "g0.4> ";				//G0.4 prompt

void readkeywords() //Reads keywords of the shared files for searching
{
	ifstream fl;
	fl.open(local_files.c_str());
	string line;
	int counter=0;


	
	while(getline(fl,line))
	{
		int keywordcounter=0;
		char *temp =(char*) line.c_str();
		char *filename = strtok(temp,":");
		searchfiles[counter][keywordcounter] = (char*) malloc(50);
		strcpy(searchfiles[counter][keywordcounter],filename);
		char *tag;
		tag = (char*) malloc(50);
		tag = strtok(NULL,"|");
		keywordcounter=1;
		while(tag!=NULL)
		{
			searchfiles[counter][keywordcounter] = (char*) malloc(50);
			strcpy(searchfiles[counter][keywordcounter],tag);
			keywordcounter++;
			tag=strtok(NULL,"|");
			
		}
		counter++;
	}	
}

//loadConfig() reads the g0.4.conf file and loads the variables
int loadConfig()
{
	string line;						//for storing the line read from the conf file
	string values[10];					//for storing the values read from the conf file
	ifstream fl;
	fl.open("g04.conf");
	
	int i=0;							//counter for values
	int pos;
	
	while(getline(fl,line))		
	{
		pos=line.find("=");
		values[i] = line.substr(pos+1);
		i++;		
	}

//Assigning Values from the conf file to global variables
	stringstream ss(values[0]);
	ss >> neighbor_port;

	//neighbor_port = atoi(values[0].c_str());
	
	stringstream ss1(values[1]);
	ss1 >> file_port;

	stringstream ss2(values[2]);
	ss2 >> number_peers;

	stringstream ss3(values[3]);
	ss3 >> TTL;
	
	stringstream ss4(values[4]);
	ss4 >> is_seed;
	
	host_cache = values[5];
	backup_cache = values[6];
	seed_nodes = values[7];
	local_files = values[8];
	local_file_directory = values[9];

	readkeywords();
	
	if(is_seed==1)
	{
		number_peers=20;
	}

	
	return 0;
	
}

struct message* createPing()
{
	struct message *ping;
	ping = (struct message*) malloc(sizeof(struct message));
	srand (time(NULL));
	ping->mid[0] = rand();	
	ping->mid[1] = rand();
	ping->mid[2] = rand();
	ping->mid[3] = rand();
	ping->pid=0;		//protocol id for ping=0
	ping->ttl = TTL;
	ping->hops=0;
	ping->plength=0;
	return ping;
}

void forwardPing(int sockfd,struct message ping)
{
	for(int i=0;i<number_peers;i++)
	{
		if(sockfd_clt[i]!=-1 && sockfd_clt[i]!=sockfd)
		{ 
			send(sockfd_clt[i],&ping,sizeof(ping),0);
			cout<<"Ping forwarded !"<<endl;
		}
	}
}

int increase_neigh_count(int x)
{
	/*
	 * x=0 means increase neighbour count
	 * x=1 means decrease neighbour count
	 * x=2 means to return the current neighbourhood count
	 * */
	pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);
	int y;
	if(x==0 && neigh_count<number_peers)
	{
    neigh_count++;
		y=0;
	}
	else if(x==1 && neigh_count>0)
	{
		neigh_count--;
		y=0;
	}
	else if(x==2)
		y=neigh_count;
	
	else
		y= -1;
	
	pthread_mutex_unlock(&mutex);
	return y;
}



void startServer_neigh()
{
	int cli_sockfd, *sock_ptr;
	struct sockaddr_in serv_addr_neigh;
		
	//Socket creation and value fill up for neighbourhood listening
	sockfd_serv_neigh = socket(AF_INET, SOCK_STREAM, 0);
	bzero((void*)&serv_addr_neigh, sizeof(serv_addr_neigh));
	serv_addr_neigh.sin_family = AF_INET;
    serv_addr_neigh.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr_neigh.sin_port = htons(neighbor_port);

	

		
	if(bind(sockfd_serv_neigh, (struct sockaddr *)&serv_addr_neigh, sizeof(serv_addr_neigh))<0)
		{
		perror("Neighbourhood Server: can't get name");
      	_exit(1);
		}

	sock_neigh_len = sizeof(serv_addr_neigh);
	int opt = 1;
	setsockopt(sockfd_serv_neigh,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));

	if (listen(sockfd_serv_neigh, 5) < 0)
	{
    perror(": bind");
    exit(1);
	}
	
}

void startServer_file()
{
		int cli_sockfd, *sock_ptr;
		struct sockaddr_in serv_addr_file;
			
	//Socket creation and value fill up for file transfer request listening
	sockfd_serv_file = socket(AF_INET, SOCK_STREAM, 0);
	bzero((void*)&serv_addr_file, sizeof(serv_addr_file));
	serv_addr_file.sin_family = AF_INET;
    serv_addr_file.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr_file.sin_port = htons(file_port);

	if(bind(sockfd_serv_file, (struct sockaddr *)&serv_addr_file, sizeof(serv_addr_file))<0)
			{
		perror("File Server: can't get name");
      	_exit(1);
		}

	sock_file_len = sizeof(serv_addr_file);

	int opt = 1;
	setsockopt(sockfd_serv_file,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));
	
	if (listen(sockfd_serv_file, 5) < 0)
	{
		
    perror(": bind");
    exit(1);
	}
}

void *startsending(void*arg)
{

	int j=0; 
	int rec_sockfd = *(int*)arg;
	char buf[MAXBUFLEN];
	struct sockaddr_in rec_addr;
	int x;
	for(int i=0;i<MAXBUFLEN;i++)
		buf[i]='\0';
					
	x=read(rec_sockfd,buf,MAXBUFLEN);
	buf[x]='\0';
	cout<<buf<<endl;
	if(x==0)
	{
	close(rec_sockfd);
	cout<<"Client closed !"<<endl<<prompt;
	}
	else if(x>0)
	{
	struct stat st; 
	char *get = strtok(buf,"/");	
	strtok(NULL,"/");	//discarding "get" field in the GET message
	char *fln	= strtok(NULL,"/");
	char *grb = strtok(NULL,":");
	char *directory;
	char path[150];
						
	directory = (char*) malloc(100);
	directory = (char*) local_file_directory.c_str();
	if(directory[strlen(directory)-1]!='/')
	{
		strcat(directory,"/");
	}
		sprintf(path,"%s%s",directory,fln);
						
	if(strcmp(get,"GET ")==0 && stat(path,&st) == 0 && strcmp(grb," HTTP/1.0\r\nUser-Agent")==0)	//received a GET message and the file exists
	//if(strcmp(grb," HTTP/1.0\r\nUser-Agent")==0)
	{	char okmg[150];
		sprintf(okmg,"HTTP 200 OK\r\nServer: Gnutella\r\nContent-type: application/binary\r\nContent-length: %lu\r\n\r\n",st.st_size);
		write(rec_sockfd,okmg , strlen(okmg));
		//Start Sending the file data

		 //ifstream myfile (path);
		char line[MAXBUFLEN];
		FILE *fp;
		fp = fopen(path,"r");
 		 if (fp!=NULL)
  		{
    	while (fgets(line,MAXBUFLEN,fp)>0)
    	{
			//cout<<"Sent line: "<<line<<endl;
      	write(rec_sockfd,line,MAXBUFLEN);
    	}
    	fclose(fp);
  		}
		close(rec_sockfd);
		
		
	}
	else	//either the Get message is garbage or the file doesn't exist
	{
		perror("Either the Get message is garbage or the file doesn't exist :");
		close(rec_sockfd);
	}

}	
					else
					{
						perror("Error Reading from file Server socket:");
					}
	
}


void *startdownload(void*arg)
{
	int sindex;
//	char *tempname;
	int templength;

	struct argument *download;
	download = (struct argument*) arg;
	sindex = download->index;
	templength = download->len;

	char *temp;
	temp =(char*)malloc(templength);
	strcpy(temp,download->fl);
	

	char *ipp = strtok(store[sindex].location,":");
	char *pport = strtok(NULL,":");
	struct addrinfo hints,*res,*ressave;
	int sockfd,rv;
		
		memset(&hints, 0, sizeof(hints));
   		hints.ai_family = AF_INET;
   		hints.ai_socktype = SOCK_STREAM;

		if ((rv=getaddrinfo(ipp,pport,&hints,&res))!=0) 
	{
		fprintf(stderr, "%s\n", gai_strerror(rv));

	}
	 else {
	  ressave = res;
			do 
		 {
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
	    continue;
	}
	if (connect(sockfd, res->ai_addr, res->ai_addrlen)==0) 
	{
		char msg[MAXBUFLEN];

		//create send message
		char sendmsg[100];
		sprintf(sendmsg,"GET /get/%s/ HTTP/1.0\r\nUser-Agent: Gnutella\r\n\r\n",store[sindex].filename);

		//create ok message
		char okget[150];
		sprintf(okget,"HTTP 200 OK\r\nServer: Gnutella\r\nContent-type: application/binary\r\nContent-length: %s\r\n\r\n",store[sindex].filesize);
		
		write(sockfd,sendmsg,strlen(sendmsg));
		if (read(sockfd, msg,strlen(okget))>0) 
		{
			if(strcmp(msg,okget)==0)
			{	
				//Create new file here
			ofstream myfile;
 			myfile.open(temp);
 			// FILE *fp;
			//fp =fopen(temp,"w");
 			//	char *abcd;
				//abcd =(char*) malloc(MAXBUFLEN+1);
				while(read(sockfd, msg,MAXBUFLEN)>0)
				{
					//cout<<"Received: "<<msg<<endl;
					//fwrite(abcd,sizeof(char),MAXBUFLEN,fp);
					myfile<<msg		;
				}
			//	fclose(fp);
				myfile.close();
			}
			else
					close(sockfd);
		}
		else
				close(sockfd);
	}
		else
				close(sockfd);

		 } while ((res = res->ai_next) != NULL);
	  freeaddrinfo(ressave);
  }
}

void *tryconnect(void *arg)
{
	pthread_detach(pthread_self());
	int sockfd,rv;
	string line;						//for storing the line read from the hostCache file
	string ip_str,port_str;
	int flag;
	//char* ip,port;						//for storing the values read from the line
	char file[100];
	strcpy(file,host_cache.c_str());		//copying hostCache file into the char[] file
	ifstream fl;
	
	int i=0;							//counter for values
	int pos;
	while(flag!=1)
	{
		fl.open(file);
	while(getline(fl,line))		
	{	
		sleep(1);
		while(increase_neigh_count(2)>=number_peers) 
		{
			sleep(5);
		}
		pos=line.find(" ");
		ip_str = line.substr(0,pos);
		port_str = line.substr(pos+1);

		//converting string to char*
		char *ip=new char[ip_str.size()+1];
		ip[ip_str.size()]='\0';
		memcpy(ip,ip_str.c_str(),ip_str.size());

		//converting string to char*
		char *port=new char[port_str.size()+1];
		port[port_str.size()]='\0';
		memcpy(port,port_str.c_str(),port_str.size());
		
		cout<<"connecting to "<<ip<<","<<port<<endl;
		
		struct addrinfo hints,*res,*ressave;
		
		memset(&hints, 0, sizeof(hints));
   		hints.ai_family = AF_INET;
   		hints.ai_socktype = SOCK_STREAM;

		if ((rv=getaddrinfo(ip,port,&hints,&res))!=0) {
		fprintf(stderr, "%s\n", gai_strerror(rv));

		}
			 
  else {
	  ressave = res;
			do {
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
	    continue;
	}
	if (connect(sockfd, res->ai_addr, res->ai_addrlen)==0) 
		{
		char msg[MAXBUFLEN];
		write(sockfd, "GNUTELLA CONNECT/0.4\r\n" ,strlen("GNUTELLA CONNECT/0.4\r\n"));
		if (read(sockfd, msg,MAXBUFLEN)>0) 
		{
	    	printf("Server response : %s--", msg);
			if(strcmp(msg,"GNUTELLA OK\r\n")==0)
			{
				int j=0;
				for(j=0;j<number_peers;j++)
				    if(sockfd_clt[j]==-1) break;
				if(j>number_peers)
				{
					cout<<"Too many Peers. Hence Connection Rejected"<<endl;
					close(sockfd);
				}
					
				else if(increase_neigh_count(0)==0)
				{
						sockfd_clt[j]=sockfd;
						FD_SET(sockfd_clt[j], &allset);
						//rset = allset;
						cout<<"connected"<<endl;
				}
				else
					close(sockfd);
			}
			else
					close(sockfd);
		}
		else
				close(sockfd);
	}
		else
				close(sockfd);
	
    } while ((res = res->ai_next) != NULL);
	  freeaddrinfo(ressave);
  }
    
    	}//inner while ends here

		
		if(strcmp(file,seed_nodes.c_str())==0)
		{
		flag=1;
		}
		
		else if(strcmp(file,backup_cache.c_str())==0)
		{
		fl.close();
		strcpy(file,seed_nodes.c_str());
		}

		else if(strcmp(file,host_cache.c_str())==0)
		{
			fl.close();
			strcpy(file,backup_cache.c_str());
			
			//fl.open(file);
		}

		
	}//outer while ends here
		fl.close();

	while(is_seed!=1)
	{		
		
	//	cout<<"Current neighbour count = "<<increase_neigh_count(2)<<endl;
		//cout<<" neighbour limit = "<<number_peers<<endl;
		while(increase_neigh_count(2)>=number_peers || increase_neigh_count(2)==0) 
		{
			sleep(1);
		}
		struct message ping;
		//ping = (struct message*) malloc(sizeof(struct message));
		//ping = createPing();
		srand (time(NULL));
		ping.mid[0] = rand();
		
		mypings[ping_counter]=ping.mid[0];
		ping_counter++;
		
		ping.mid[1] = rand();
		ping.mid[2] = rand();
		ping.mid[3] = rand();
		ping.pid=0;		//protocol id for ping=0
		ping.ttl = TTL;
		ping.hops=0;
		ping.plength=0;
		for(int i=0;i<number_peers;i++)
		{
			if(sockfd_clt[i]!=-1)
			{
				send(sockfd_clt[i],&ping,sizeof(ping),0);
				cout<<"ping sent with mid = "<<ping.mid[0]<<ping.mid[1]<<ping.mid[2]<<ping.mid[3]<<endl;
			}
		}

		//free(&ping);
		sleep(30);		
		}
}
int main(int argc, char** argv)
{
	//Printing the welcome messages
	cout<<endl<<"\t\t***************************************"<<endl;
	cout<<"\t\t-------------Welcome to G0.4----------"<<endl;
	cout<<"\t\t***************************************"<<endl<<endl;	
	cout<<"Starting Server...."<<endl;
	
	if(loadConfig()<0)
	{
		perror(":config");
		_exit(0);
	}
	else
		cout<<"\t\tConfig file loaded."<<endl;

	 for (int i=0; i<number_peers; i++) sockfd_clt[i] = -1;
	
	 FD_ZERO(&allset);		//initialize allset
	
	startServer_neigh();
		cout<<"\t\tServer started for listening neighbour connections."<<endl;
	FD_SET(sockfd_serv_neigh, &allset);			//Add the neighbour server socket descriptor to the set
	
	startServer_file();
	cout<<"\t\tServer started for listening file transfer requests."<<endl<<endl;
	FD_SET(sockfd_serv_file, &allset);			//Add the file server socket descriptor to the set


	
	FD_SET(STDIN_FILENO, &allset);
	
	cout<<"\t\tYour G04 listens for new connections on Port "<<neighbor_port<<endl;
	cout<<"\t\tFiles are transfered over Port "<<file_port<<endl;
	cout<<"\tWARNING ! To change settings change the config file and restart G0.4"<<endl;

	for(int z=0;z<200;z++)
	{
		mypings[z]=-1;
		otherpings[z][0]=-1;
		otherpings[z][1]=-1;
	}
	
	pthread_t tid;
	pthread_create(&tid, NULL, &tryconnect, NULL);	
	//cout<<prompt;
	
	
	
	while(1)
	{
		fflush(stdout);

	cout<<prompt;
		rset = allset;
		int select_retval = select(64, &rset, NULL, NULL, NULL);
		if (select_retval < 0)
		{
			perror("select");
		} else if (select_retval > 0)
		{
			if (FD_ISSET(STDIN_FILENO, &rset))	//If the input is from standard input
			{
				strcpy(input,"\0");
				if (fgets(input, 100, stdin) == NULL) exit(0);
				input[strlen(input)-1]='\0';
					if(strlen(input)>1)
				{
				char* cmd = strtok(input," ");
					
				char* next = strtok(NULL,"+++++");
				if(strcmp(cmd,"search")==0)
					{
						if(next!=NULL)
						{
							cout<<"Searching for "<<next<<endl;
							int hitcount=indexcounter;
							
							int outer=0;
							while(outer<50)
							{
								int inner=1;
								while(inner<20)
								{
									if( searchfiles[outer][inner]!=NULL && strcmp(searchfiles[outer][inner],next)==0)
									{
										
										cout<<"Index:"<<hitcount<<"\tFilename: "<<searchfiles[outer][0];
											struct stat st;
											char filename[120];
											char *directory;
											directory = (char*) malloc(100);
										directory = (char*) local_file_directory.c_str();
										if(directory[strlen(directory)-1]!='/')
										{
											strcat(directory,"/");
										}
										 sprintf(filename,"%s%s",directory,searchfiles[outer][0]);
    										if (stat(filename, &st) == 0)
        									cout<<"\t Size: "<<st.st_size;
											cout<<"\t Location: Local"<<endl;
										hitcount++;
										break;
									} //inner while ends here
									inner++;
								}
								outer++;
							}//outer while ends here

							indexcounter = hitcount;
							//QUERY GENERATION starts here
							struct message query;
							//query = (struct message*) malloc(sizeof(struct message));
							srand (time(NULL));
							query.mid[0] = rand();	
							query.mid[1] = rand();
							query.mid[2] = rand();
							query.mid[3] = rand();
							//Entering mid of the query message sent and also keeping track of the Index to be displayed
							myqueries[querycounter] = query.mid[0];
							
							querycounter++;
							//
							query.pid=2;		//protocol id for ping=0
							query.ttl = TTL;
							query.hops=0;
							query.plength=sizeof(next);
							strcpy(query.payload,next);

							for(int zz=0;zz<number_peers;zz++)
							{
								if(sockfd_clt[zz]!=-1)
								{
								send(sockfd_clt[zz],&query,sizeof(query),0);
							//	cout<<"QUERY message sent!"<<endl;
								}
							}
						}
						else
						{
							cout<<"no keywords given to search for."<<endl;
						}
					}
					else if(strcmp(cmd,"get")==0)
					{
						
						int ind;
						if(next!=NULL && (ind=atoi(next))>0)
						{
						
							char *tempname;
						//	tempname = (char*) malloc(100);
							strtok(next," ");
							tempname = strtok(NULL," ");
							
						//	cout<<"tempname = "<<tempname<<endl;
						//	cout<<"no malloc still working ! "<<endl;
							for(int a=0;a<50;a++)
							{
								if(store[a].index==ind)
								{
								//	cout<<"Found your file to download"<<endl;

									if(strcmp(store[a].location,"Local")==0)
									{
										cout<<"Skipping download as file you chose is in Local store"<<endl;
									}
									else
									{
										cout<<"Entered else"<<endl;
										pthread_t tid2;
											
										if(tempname!=NULL)
										{
									//	cout<<"tempname for the downloading file is "<<tempname<<endl;
											//startdownload(a,tempname,strlen(tempname));
											struct argument dwn;
											dwn.index =a;
											strcpy(dwn.fl,tempname);
											dwn.len = strlen(tempname);
										//	cout<<"elements :"<<dwn.index<<":"<<dwn.fl<<":"<<dwn.len<<endl;
											pthread_create(&tid2, NULL, &startdownload, (void*)&dwn);
										}
										else
										{
										//	cout<<"trying to download "<<store[a].filename<<endl;
											struct argument dwn;
											dwn.index =a;
											strcpy(dwn.fl,store[a].filename);
											dwn.len = strlen(store[a].filename);
										//	cout<<"elements :"<<dwn.index<<":"<<dwn.fl<<":"<<dwn.len<<endl;
											pthread_create(&tid2, NULL, &startdownload,(void*) &dwn);
										}
									
									}
								}
							}
						}
						else
							cout<<"enter a valid index"<<endl;
					}
				else if(strcmp(cmd,"exit")==0)
					{
						//Writing Ips and port numbers to backuphostcache file
					//	ifstream fl;
						//fl.open();
		 			exit(0);
					}
				}
				//cout<<prompt;
			}

			 if (FD_ISSET(sockfd_serv_neigh, &rset))		//If we need to read from neighbour server
			{
				int j=0, rec_sockfd;
				char buf[MAXBUFLEN];
				struct sockaddr_in rec_addr; 
				for(j=0;j<number_peers;j++)
					if(sockfd_clt[j]==-1) break;

				if((rec_sockfd= accept(sockfd_serv_neigh, (struct sockaddr *)(&rec_addr), &sock_neigh_len)) >= 0)
				{
					cout <<"remote machine ="<<inet_ntoa(rec_addr.sin_addr) <<", port = "<< ntohs(rec_addr.sin_port)<<endl;
   
      				if (rec_sockfd< 0) 
					{
					perror(": accept");
					exit(1);
      				}

					if(j>number_peers) //if(j==number_peers-1)
					{
						cout<<"Too many Peers. Hence Connection Rejected"<<endl;
						close(rec_sockfd);
					}					

					else
					{
					int x;
						for(int i=0;i<MAXBUFLEN;i++)
							buf[i]='\0';
						
					x=read(rec_sockfd,buf,MAXBUFLEN);
				  	  buf[x]='\0';
						cout<<buf<<endl;
					if(x==0)
					{
						close(rec_sockfd);
						cout<<"Client closed"<<endl;
					}
						if(x>0)
						{
					 if(strcmp(buf,"GNUTELLA CONNECT/0.4\r\n")==0)
					{						
						write(rec_sockfd, "GNUTELLA OK\r\n", strlen(ok_msg));
						if(increase_neigh_count(0)==0)
						{
						sockfd_clt[j]=rec_sockfd;
						FD_SET(sockfd_clt[j], &allset);
						}
					}
					}
					}
				}
			}

			 if(FD_ISSET(sockfd_serv_file, &rset))		//If we need get read file server
			{
				//cout<<"File server";
				int j=0, rec_sockfd;
				char buf[MAXBUFLEN];
				struct sockaddr_in rec_addr;
				if((rec_sockfd= accept(sockfd_serv_file, (struct sockaddr *)(&rec_addr), &sock_file_len)) >= 0)
				{
					pthread_t tid3;
					cout <<"remote machine ="<<inet_ntoa(rec_addr.sin_addr) <<", port = "<< ntohs(rec_addr.sin_port)<<" connected for file transfer"<<endl;
   					pthread_create(&tid3, NULL, &startsending,(void*) &rec_sockfd);
				}//accept IF ends here  
				else
				{
					perror("Error accepting connections");
					close(rec_sockfd);
				}
			}

			for (int g=0; g<number_peers; g++) 	
			{
				 if (sockfd_clt[g] < 0) continue;
				if(FD_ISSET(sockfd_clt[g], &rset))
				{
					char msg[MAXBUFLEN];
					int x;
					for(int i=0;i<MAXBUFLEN;i++)
							msg[i]='\0';
					struct message rcv;
									
				//ptr = (struct message*) malloc(sizeof(struct message*));
				//	x=read(sockfd_clt[g],msg,MAXBUFLEN);
					x=recv(sockfd_clt[g],&rcv,sizeof(struct message),0);
					if(x==0)
					{
						if(increase_neigh_count(1)==0)
						{
						cout<<"Peer Closed !"<<endl;
						FD_CLR(sockfd_clt[g], &allset);
						close(sockfd_clt[g]);
							
						sockfd_clt[g] = -1;
						}
						
					}
					else
					{
					msg[x]='\0';
				//cout<<"Client "<<sockfd_clt[g]<<" says: "<<rcv.mid[0]<<" "<<rcv.mid[1]<<" "<<rcv.mid[2]<<" "<<rcv.mid[3]<<", pid ="<<rcv.pid<<endl;
						//printf("Client %d says: ");
					if(rcv.pid==0)		//PING !!
						{
							cout<<"Ping Recieved from"<<sockfd_clt[g]<<" with message id = "<<rcv.mid[0]<<rcv.mid[1]<<rcv.mid[2]<<rcv.mid[3]<<endl;
							//Check if we somehow got our own ping back.
							int a=0;
							while(rcv.mid[0]!=mypings[a] && rcv.mid[0]!=otherpings[a][0])
							{
								if(a==199) break;
							//	cout<<"a="<<a<<"recieved mid="<<rcv.mid[0]<<" mypings[a]="<<mypings[a]<<endl;
								a++;
							}

							if(a<199)
							{
								//Received my own ping so Do Nothing.
								cout<<"Received a ping which is my own or I already forwareded this ping. Discarded!"<<endl;
							}
							
							else //Someone else sent a ping
							{
								if(rcv.hops!=0 && increase_neigh_count(2)<number_peers)	//Check if number of peers are full
						{
							//Send a pong using same MessageID
							struct message snd;
							snd.mid[0]=rcv.mid[0];
							snd.mid[1]=rcv.mid[1];
							snd.mid[2]=rcv.mid[2];
							snd.mid[3]=rcv.mid[3];
							snd.pid=1;		//Pong's pid
							snd.ttl=rcv.hops+1;
							snd.hops=0;
							//Get own Ip address
							char Buf [ 200 ] ;
							struct hostent * Host = (struct hostent * ) malloc ( sizeof ( struct hostent ));
							gethostname ( Buf , 200 ) ;
							//printf ( "%s\n", Buf ) ;
							Host = ( struct hostent * ) gethostbyname ( Buf ) ;
//							printf ( "The name :: %s\n" , Host->h_name ) ;
							//char *payload;
							//snd.payload =  malloc(sizeof(neighbor_port)+sizeof(inet_ntoa(*((struct in_addr *)Host->h_addr)))+2);
							//strcpy(pa,inet_ntoa(*((struct in_addr *)Host->h_addr)));
							sprintf(snd.payload,"%s:%d",inet_ntoa(*((struct in_addr *)Host->h_addr)),neighbor_port);
							//printf("\nPayload: %s\n", snd.payload);
							snd.plength=sizeof(snd.payload);
							send(sockfd_clt[g],&snd,sizeof(snd),0);
							cout<<"Pong Sent"<<endl;
						}
						rcv.ttl--;
						rcv.hops++;
						if(rcv.ttl>0)
								{
									otherpings[other_counter][0]=rcv.mid[0];
									otherpings[other_counter][1]=sockfd_clt[g];
									other_counter++;
							forwardPing(sockfd_clt[g],rcv);
								}
					fflush(stdout);
					//write(sockfd_clt[g], msg, strlen(msg));
					break;
							}
						}
						else if(rcv.pid==1)		//In case of receiving a PONG
						{
							cout<<"Received a PONG !"<<endl;
							int a=0;
							while(a<200)	//Checking if the received Pong message is a reply to own Ping message
							{
								if(rcv.mid[0]==mypings[a])	
								{
									char *payload;
									payload = (char*) malloc(rcv.plength);
									strcpy(payload,rcv.payload);
									char *ip = strtok(payload,":");
									char *port = strtok(NULL,":");
									if(increase_neigh_count(2)<number_peers)
									{
								//	cout<<"Its a reply to my own Ping"<<endl;
									int sockfd,rv;

									struct addrinfo hints,*res,*ressave;
		
									memset(&hints, 0, sizeof(hints));
   									hints.ai_family = AF_INET;
   									hints.ai_socktype = SOCK_STREAM;

									if ((rv=getaddrinfo(ip,port,&hints,&res))!=0) 
									{
										fprintf(stderr, "%s\n", gai_strerror(rv));
									}
			 						ressave = res;

									 do 
									{
									if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) 
										{
	    									continue;
										}
									if (connect(sockfd, res->ai_addr, res->ai_addrlen)==0) 
									{
									char msg[MAXBUFLEN];
									write(sockfd, "GNUTELLA CONNECT/0.4\r\n" ,strlen("GNUTELLA CONNECT/0.4\r\n"));
									if (read(sockfd, msg,MAXBUFLEN)!=0) 
									{
	    							//printf("Server response : %s--", msg);
									if(strcmp(msg,"GNUTELLA OK\r\n")==0)
									{
									int j=0;
									for(j=0;j<number_peers;j++)
										{
				    				if(sockfd_clt[j]==-1) break;
										}
									if(j>number_peers)
									{
									cout<<"Too many Peers. Hence Connection Rejected"<<endl;
									close(sockfd);
									}
										
									else if(increase_neigh_count(0)==0)
									{
									sockfd_clt[j]=sockfd;
									FD_SET(sockfd_clt[j], &allset);
									//rset = allset;
									//cout<<"connected"<<endl;
									}
									else		//If neighbour count cannot be increased !
										{
											cout<<"ERROR: Neighbourhood count cannot be increased"<<endl;
									close(sockfd);
										}
									}
									else
										{
											cout<<"ERROR: Ok message not in proper order"<<endl;
									close(sockfd);	// message snet was not gnutella OK
										}
									}
									else
										{
											cout<<"ERROR: Socket closed from the other side"<<endl;
									close(sockfd);	// if socket reads 0 i.e. it was closed from other side
										}
									}
									else
										{
											cout<<"ERROR:Connect failed"<<endl;
									close(sockfd);	//couldn't connect
										}
									} while ((res = res->ai_next) != NULL);
   									 freeaddrinfo(ressave);
									}
									else
									{
										strcpy(pongIPstore[pongcounter],ip);
										pongPORTstore[pongcounter]=atoi(port);
										pongcounter++;
									}
									
									break;
								}
										if(rcv.mid[0]==otherpings[a][0])
										{
											//cout<<"Found a match to forward the Pong to"<<endl;
											int q=0;
										for(q=0;q<number_peers;q++)
											{
												if(sockfd_clt[q]==otherpings[a][1])
													break;
											}
											if(q<number_peers)
											{
										rcv.ttl--;
										rcv.hops++;
												if(rcv.ttl!=0)
										send(otherpings[a][1],&rcv,sizeof(rcv),0);
												cout<<"Pong forwarded"<<endl;
											break;
											}
										}
									a++;
							}
							
						}

						else if(rcv.pid==2)	//QUERY !
						{
							cout<<"received query!"<<endl;
							int gg=0;
							while(rcv.mid[0]!=myqueries[gg] && rcv.mid[0]!=otherqueries[gg][0])
							{
								if(gg==199) break;
								gg++;
							}
							if(gg<199)	//check if its my own query or already received 
							{
								//Dont do anything received already forwared query or own back
								cout<<"received own query or already forwarded query Discard"<<endl;
							}
							else		//Fresh Query
							{
								
									//searching for the keywords in my shared directory
									char* search;
									search=(char*)malloc(rcv.plength);
									strcpy(search,rcv.payload);
									int hitcount=0;
									char *resultset;
									/**
									 * Result set format: Filename|Filesize:Filename|Filesize.....
									 * */
									resultset= (char*)malloc(1000);
							int outer=0;
							while(outer<50)
							{
								int inner=1;
								while(inner<20)
								{
									if( searchfiles[outer][inner]!=NULL && strcmp(searchfiles[outer][inner],search)==0)
									{
											struct stat st;
											char filename[120];
											char *directory;
											directory = (char*) malloc(100);
										directory = (char*) local_file_directory.c_str();
										if(directory[strlen(directory)-1]!='/')
										{
											strcat(directory,"/");
										}
										 sprintf(filename,"%s%s",directory,searchfiles[outer][0]);
										//cout<<"Trying to open file:"<<
    										if (stat(filename, &st) == 0)
										{
											hitcount++;
											if(hitcount==1)
												strcpy(resultset,searchfiles[outer][0]);
											else
											strcat(resultset,searchfiles[outer][0]);
											strcat(resultset,"|");
												char lint[30];
												sprintf(lint,"%lu",st.st_size);
        										strcat(resultset,lint);
												strcat(resultset,":");
										}
										else	cout<<"file doesnot exist"<<endl;
										//	cout<<"\t Location: Local"<<endl;
										
										break;
									} //inner while ends here
									inner++;
								}
								outer++;
							}//outer while ends here
									resultset[strlen(resultset)-1]='\0';//Removing the last character which will be ':'

									//search ends here
								if(hitcount>0)	//Create QueryHIT
									{
									struct message queryhit;
									queryhit.mid[0]=rcv.mid[0];
									queryhit.mid[1]=rcv.mid[1];
									queryhit.mid[2]=rcv.mid[2];
									queryhit.mid[3]=rcv.mid[3];

									queryhit.pid=3;
									queryhit.ttl=rcv.hops+1;
										queryhit.hops=0;
										//Creating payload
										/**
										 * Format for Payload:
										 * hits:fileserver_port:ip:resultset
										 * */
										char hname [ 200 ] ;
							struct hostent * Host = (struct hostent * ) malloc ( sizeof ( struct hostent ));
							gethostname (hname,200 ) ;
							Host = ( struct hostent * ) gethostbyname (hname);
									sprintf(queryhit.payload,"%d:%d:%s:%s",hitcount,file_port,inet_ntoa(*((struct in_addr *)Host->h_addr)),resultset);
								send(sockfd_clt[g],&queryhit,sizeof(queryhit),0);
										cout<<"QUERY HIT SENT"<<endl;
									}
								}

								rcv.ttl--;
								rcv.hops++;
								if(rcv.ttl>0)	//If TTL>0 forward the query message to the neighbours
								{
									otherqueries[other_query_counter][0]=rcv.mid[0];
									otherqueries[other_query_counter][1]=sockfd_clt[g];
									other_query_counter++;
									for(int zz=0;zz<number_peers;zz++)
									{
										if(sockfd_clt[zz]!=-1 && sockfd_clt[zz]!=sockfd_clt[g])
											send(sockfd_clt[zz],&rcv,sizeof(rcv),0);

									}

								
							}
						}//Query if loop ends here
						else if(rcv.pid==3) 
						{ 	
							cout<<"Query hit received !"<<endl;
							// QUERYHIT LOOP STARTS HERE
							//checking if the hit is for myown query
							int a=0;
							while(a<200)
							{//While loop for a starts here
								//checking if the queryhit is for my own query message
								if(rcv.mid[0]==myqueries[a])
								{
									//Unpacking the queryhit message here
									strtok(rcv.payload,":");	//returns number of hits- ignoring
									char *pport=strtok(NULL,":");
									char *ipp=strtok(NULL,":");
									char *filename;
									filename =(char*) malloc(100);
									filename = strtok(NULL,"|"); // garbage character for gettin all of the result values
									while(filename!=NULL)
									{
										store[indexcounter].index=indexcounter;
										strcpy(store[indexcounter].filename,filename);
										strcpy(store[indexcounter].filesize,strtok(NULL,":"));
										
										sprintf(store[indexcounter].location,"%s:%s",ipp,pport);

										cout<<"Index: "<<store[indexcounter].index<<"\t Filename: "<<store[indexcounter].filename<<"\t Size: "<<store[indexcounter].filesize<<"\t Location: "<<store[indexcounter].location<<endl; 
										indexcounter++;

										filename = strtok(NULL,"|");
		
									}
									
									//cout<<"Testing queryhit message set= "<<set<<endl;
								}
								else		//forward this query hit from where the query came
								{
									//checking which neighbour to forward this queryhit
									if(rcv.mid[0]==otherqueries[a][0])
									{
										rcv.ttl--;
										rcv.hops++;
										send(otherqueries[a][1],&rcv,sizeof(rcv),0);
									}
								}

								a++;
							}//while loop for a ends here
						}//QUERY HIT LOOP ENDS HERE
					}
				}
			}
	//		cout<<"Current neighbourhood count = "<<increase_neigh_count(2)<<endl;
		/*	cout<<"-----------"<<endl;
			for(int i=0;i<number_peers;i++)
				cout<<sockfd_clt[i]<<endl;
				cout<<"-----------"<<endl;*/
		}


	}
}
