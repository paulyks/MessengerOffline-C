#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#include <my_global.h>
#include <mysql.h>

#define clearScreen() printf("\033[H\033[J");fflush(stdout)
#define PORT 2728

extern int errno;
struct accounts{
	int fd;
	int id;
	char name[256];
	int status;
	int newMessage;
};
struct thData{
	int fd;
	int option;
};
int nfds;
MYSQL *con; 
struct accounts accounts[1000];
fd_set readfds;
fd_set actfds;
static void* writeToClient(void*);
void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);        
}
void query(MYSQL* con,char* text)
{
	if(mysql_query(con,text))
		finish_with_error(con);
}

int accountExist(MYSQL* con,char* buffer)
{
	char *text = malloc(strlen(buffer)+50);
	sprintf(text,"%s%s'","SELECT 1 FROM users WHERE Name='",buffer);
	query(con,text);
	MYSQL_RES *result = mysql_store_result(con);
	if(result == NULL)
		finish_with_error(con);
	int line = 0;
	MYSQL_ROW row;
	if((row = mysql_fetch_row(result)))
		line = 1;
	mysql_free_result(result);
	return line == 0?0:1;
}
void registerAcc(char* text,struct accounts *accounts,MYSQL *con)
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char *name = malloc(strlen(text));
	char *password = malloc(strlen(text));
	name = strtok(text,":");
	password = strtok(NULL,"");
	char* string = malloc(strlen(name)+strlen(password)+256);
	sprintf(string,"%s,'%d-%d-%d'","INSERT INTO users VALUES(0",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday);
	sprintf(string,"%s,'%s','%s',%d,%d)",string,name,password,1,0);
	query(con,string);
}
int loginAcc(MYSQL *con,char* buffer)
{
	char *name = malloc(strlen(buffer));
	char *password = malloc(strlen(buffer));
	name = strtok(buffer,":");
	password = strtok(NULL,"");
	char* string = malloc(strlen(buffer)+256);
	sprintf(string,"SELECT 1 FROM users WHERE Name = '%s' and Password = '%s'",name,password);
	query(con,string);
	MYSQL_RES *result = mysql_store_result(con);
	if(result == NULL)
		finish_with_error(con);
	int line = 0;
	MYSQL_ROW row;
	if((row = mysql_fetch_row(result)))
		line = 1;
	mysql_free_result(result);
	if(line == 1)
	{
		sprintf(string,"UPDATE users SET Status = 1 WHERE Name = '%s'",name);
		query(con,string);

	}
	return line == 0?0:1;
}
void disconnect(MYSQL *con,char* buffer)
{
	char* text = malloc(256);
	sprintf(text,"UPDATE users SET Status = 0 Where Name='%s'",buffer);
	query(con,text);
}
int isOnline(MYSQL *con,char *buffer)
{
	char *name = malloc(strlen(buffer));
	char *string = malloc(strlen(buffer)+256);
	char *text = malloc(strlen(buffer));
	strcpy(text,buffer);
	name = strtok(text,":");
	sprintf(string,"SELECT Status FROM users WHERE Name = '%s'",name);
	query(con,string);
	MYSQL_RES *result = mysql_store_result(con);
	if(result == NULL)
		finish_with_error(con);
	MYSQL_ROW row;
	int status = 0;
	if((row = mysql_fetch_row(result)))
		status = (status == atoi(row[0]) ? 0:1);
	return !status;
}
void populateAccounts(MYSQL *con,int fd,char *buffer)
{
	char *text = malloc(strlen(buffer)+256);
	sprintf(text,"SELECT * FROM users WHERE Name='%s'",buffer);
	query(con,text);
	MYSQL_RES *result = mysql_store_result(con);
	if(result == NULL)
		finish_with_error(con);
	MYSQL_ROW row = mysql_fetch_row(result);
	accounts[fd].fd = fd;
	accounts[fd].id = atoi(row[0]);
	strcpy(accounts[fd].name,row[2]);
	accounts[fd].status = atoi(row[4]);
	accounts[fd].status = atoi(row[5]);
}
void wAccesingOnlineAccount(char*buffer,int nfds)
{
	fflush(stdout);
	char *name = malloc(strlen(buffer));
	name = strtok(buffer,":");
	for(int i = 0;i<=nfds;i++)
	{
		fflush(stdout);
		if(strcmp(name,accounts[i].name) == 0)
		{
			int option = 100;
			write(i,&option,sizeof(option));
			break;
		}
	}
}
void sendMessageToUser(int senderID,int userID,char* buffer)
{
	int userOnline = 0,opt = 103,i,userOnlineInChat = 0;;
	buffer[strlen(buffer)-1] = '\0';
	fflush(stdout);
	for(i=0;i <= nfds; i++)
	{
		if(accounts[i].id == userID)
		{
			userOnline = 1;
			write(accounts[i].fd,&opt,sizeof(opt));
			write(accounts[i].fd,&senderID,sizeof(senderID));
			opt = strlen(buffer);
			write(accounts[i].fd,&opt,sizeof(opt));
			write(accounts[i].fd,buffer,opt);
			read(accounts[i].fd,&userOnlineInChat,sizeof(userOnlineInChat));
			break;
		}
	}
	buffer = strtok(buffer,":");
	buffer = strtok(NULL,"~");
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char *txt = malloc(opt + 300);
	if(userOnline == 1 && userOnlineInChat == 1)
		sprintf(txt,"INSERT INTO message VALUES(0,%d,%d,'%d-%d-%d',1,'%s')",senderID,userID,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,buffer);
	else
		sprintf(txt,"INSERT INTO message VALUES(0,%d,%d,'%d-%d-%d',0,'%s')",senderID,userID,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,buffer);
	query(con,txt);
}
int exitFromChatOccured(char* txt)
{
	txt = strtok(txt,":");
	txt = strtok(NULL,"\n");
	if(txt[0] == '0')
		return 1;
	return 0;
}
void chatWithHasLeftConversation(int userID)
{
	int opt = 104;
	for(int i=0;i<=nfds;i++)
		if(accounts[i].id == userID)
		{
			write(accounts[i].fd,&opt,sizeof(opt));
			break;
		}
}
void sendMessageToLobby(char* buf,char* buf2,char* thrdBuf)
{
	char* text,*text2,*text3;
	MYSQL_RES *result;
	MYSQL_ROW row;
	int bufferSize;
	text = malloc(300 + strlen(buf2));
	int msg = 108;
	sprintf(text,"SELECT User FROM chatRooms WHERE Name ='%s' AND User != (SELECT ID FROM users WHERE Name = '%s')",buf2,buf);
	query(con,text);
	result = mysql_store_result(con);
	if(result == NULL)
		finish_with_error(con);
	text[0] = '\0';
	if((row = mysql_fetch_row(result)))
	{
		text = malloc(strlen(row[0]));
		sprintf(text,"%s",row[0]);
	}
	while((row = mysql_fetch_row(result)))
	{
		text2 = malloc(strlen(row[0]) + strlen(text));
		sprintf(text2,"%s,%s",text,row[0]);
		text = malloc(strlen(text2));
		strcpy(text,text2);
	}
	if(text[0] != '\0')
	{
		text3 = malloc(strlen(buf) + strlen(buf2) + strlen(thrdBuf)+100);
		if(thrdBuf[0] == '0' && thrdBuf[1] == '\0')
			sprintf(text3,">%s has been disconnected from lobby.\n",buf);
		else
			if(thrdBuf[0] == '\0')
				sprintf(text3,">%s has joined to lobby.\n",buf);
			else
				sprintf(text3,"%s:%s",buf,thrdBuf);
		fflush(stdout);
		bufferSize = strlen(text3);
		for(int i=0;i<=nfds;i++)
		{
			text2 = malloc(strlen(text));
			strcpy(text2,text);
			text2 = strtok(text2,",");
			while(text2)
			{
				if(accounts[i].id == atoi(text2))
				{
					write(accounts[i].fd,&msg,sizeof(msg));
					write(accounts[i].fd,&bufferSize,sizeof(bufferSize));
					write(accounts[i].fd,text3,bufferSize);
				}
				text2 = strtok(NULL,",");
			}
		}
	}
}
int main ()
{
	clearScreen();
	char *buffer,*buffer2,*text;
	
	struct sockaddr_in server;
	struct sockaddr_in from;
	
	struct timeval tv;
	int sd,client;
	int option,bufferSize;
	struct thData thD;
	bzero(accounts,sizeof(accounts));
	printf("Program %d started.\n",getpid());
	fflush(stdout);
	printf("MySQL client version: %s\n", mysql_get_client_info());
	fflush(stdout);
	con = mysql_init(NULL);
	if (con == NULL) 
  	{
    	fprintf(stderr, "%s\n", mysql_error(con));
    	exit(1);
  	}
  	if(mysql_real_connect(con, "localhost", "root", "", "MessengerDB", 0, NULL, 0) == NULL) 
    	finish_with_error(con);
    printf("[SERVER] MySQL connection has been succesfuly done.\n");
    fflush(stdout);
    bzero(&accounts,sizeof(accounts));
    query(con,"UPDATE users SET Status = 0 WHERE Status > 0");
    query(con,"TRUNCATE TABLE chatRooms");
    query(con,"INSERT INTO chatRooms VALUES(0,'General',-1,'0')");
    
    if((sd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
    	perror("[SERVER] Socket error!\n");
    	return errno;
    }
    bzero(&server,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    if(bind(sd,(struct sockaddr*)&server,sizeof(struct sockaddr)) == -1)
    {
    	perror("[SERVER] Bind erro!\n");
    	return errno;
    }
    if(listen(sd,5) == -1)
    {
    	perror("[SERVER] Listen error!\n");
    	return errno;
    }
    FD_ZERO(&actfds);
    FD_SET(sd,&actfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    nfds = sd;
    int len,fd;
    printf("[SERVER] Messenger by Paul is up an ready for connections at port %d.\n",PORT);
    fflush(stdout);
    pthread_t th;
    while(1)
    {
    	bcopy((char*)&actfds,(char*)&readfds,sizeof(readfds));
    	if(select(nfds+1,&readfds,NULL,NULL,&tv) < 0)
    	{
    		perror("[SERVER] Select error!\n");
    		return errno;
    	}
    	if(FD_ISSET(sd,&readfds))
    	{
    		len = sizeof(from);
    		bzero(&from,sizeof(from));
    		client = accept(sd,(struct sockaddr*)&from,&len);
    		if(client < 0)
    		{
    			perror("[SERVER] Accept error!\n");
    			continue;
    		}
    		if(nfds < client)
    			nfds = client;
    		FD_SET(client,&actfds);
    		fflush(stdout);
    	}
    	for(fd = 0; fd <= nfds; fd++)
    	{
    		if(fd != sd && FD_ISSET(fd,&readfds))
    		{
    			bzero(&option,sizeof(option));
    			read(fd,&option,sizeof(option));
    			switch(option)
    			{
    				case -1:
						FD_CLR(fd,&actfds);
						read(fd,&option,sizeof(option));
						if(option == 1)
						{
							read(fd,&option,sizeof(option));
							if(option != -1)
								chatWithHasLeftConversation(option);
							read(fd,&bufferSize,sizeof(bufferSize));
							buffer = malloc(bufferSize);
							read(fd,buffer,bufferSize);
							buffer[bufferSize] = '\0';
							read(fd,&option,sizeof(option));
							if(option)
							{
								read(fd,&bufferSize,sizeof(bufferSize));
								buffer2 = malloc(bufferSize);
								read(fd,buffer2,bufferSize);
								buffer2[bufferSize] = '\0';
								text = malloc(3);
								text[0] = '0';
								text[1] = '\0';
								sendMessageToLobby(buffer,buffer2,text);
								text = malloc(300+strlen(buffer));
								sprintf(text,"DELETE FROM chatRooms WHERE User = (SELECT ID FROM users WHERE Name = '%s')",buffer);
								query(con,text);
							}
							disconnect(con,buffer);
						}
						bzero(&accounts[fd],sizeof(accounts[fd]));
						close(fd);
						break;
					default:
						bzero(&thD,sizeof(thD));
						thD.fd = fd;
						thD.option = option;
						pthread_t th;
						FD_CLR(fd,&actfds);
						pthread_create(&th,NULL,&writeToClient,&thD);
						sleep(0.3);
						break;
    			}
    			
    		}
    		FD_CLR(fd,&readfds);
    	}
    }
    mysql_close(con);
	return 1;
}

static void* writeToClient(void* arg)
{
	struct thData thD;
	thD = (*(struct thData*)arg);
	int bufferSize,index;
	int id1,id2;
	char* buffer;
	char* buffer2;
	char* text,*text2,*text3;
	MYSQL_RES *result;
	MYSQL_ROW row;
	switch(thD.option)
	{
		
		case 0://login
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			fflush(stdout);
			if(accountExist(con,buffer) != 0)
			{
				thD.option = 5;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			else
			{
				thD.option = 7;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			break;
		case 1: //register
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			fflush(stdout);
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			if(accountExist(con,buffer) != 0)
			{
				thD.option = 2;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			else
			{
				thD.option = 3;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			break;
		case 2://show onlie users
			query(con,"SELECT Name FROM users WHERE Status > 0");
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
			{
				buffer2 = malloc(strlen(row[0]));
				strcpy(buffer2,row[0]);
			}
			while((row = mysql_fetch_row(result)))
			{
				buffer = malloc(strlen(row[0]) + strlen(buffer2)+5);
				sprintf(buffer,"%s,%s",buffer2,row[0]);
				buffer2 = malloc(strlen(buffer));
				strcpy(buffer2,buffer);
			} 
			bufferSize = strlen(buffer2);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,buffer2,bufferSize);
			break;
		case 3: // add friend
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			char* name = malloc(bufferSize);
			read(thD.fd,name,bufferSize);
			name[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			char *chatWith = malloc(bufferSize);
			read(thD.fd,chatWith,bufferSize);
			chatWith[bufferSize] = '\0';
			text = malloc(strlen(chatWith)+strlen(name) + 250);
			sprintf(text,"SELECT ID FROM users WHERE Name = '%s' UNION SELECT ID FROM users WHERE Name ='%s'",name,chatWith);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
				id1 = atoi(row[0]);
			if((row = mysql_fetch_row(result)))
				id2 = atoi(row[0]);
			mysql_free_result(result);
			text = malloc(256);
			sprintf(text,"SELECT 1 from friends WHERE ID = %d and Friend = %d",id1,id2);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
			{
				//exista deja prietenia
				thD.option = 101;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			else
			{
				text = malloc(256);
				sprintf(text,"INSERT INTO friends VALUES(%d,%d)",id1,id2);
				query(con,text);
				thD.option = 102;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			break;
		case 4: //openChat
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(bufferSize+150);
			sprintf(text,"SELECT ID FROM users Where Name = '%s'",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
				id1 = atoi(row[0]);
			write(thD.fd,&id1,sizeof(id1));
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize] = '\0';
			text = malloc(bufferSize+150);
			sprintf(text,"SELECT ID FROM users Where Name = '%s'",buffer2);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
				id2 = atoi(row[0]);
			write(thD.fd,&id2,sizeof(id2));
			//sending messages
			text = malloc(300);
			sprintf(text,"SELECT `From`,`To`,Date,Seen,Message FROM message WHERE (`From` = %d AND `To` = %d) OR (`From` = %d AND `To` = %d) ORDER BY ID DESC",id1,id2,id2,id1);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			index = 0;
			text3 = malloc(50);
			sprintf(text3,">\x1B[31mEnter 0 to exit from this conversation.\x1B[0m");
			while((row = mysql_fetch_row(result)) && index < 10)
			{
				text = malloc(strlen(row[4])+strlen(buffer)+strlen(buffer2));
				if(atoi(row[1]) == id2)
					sprintf(text,"%s:%s",buffer2,row[4]);
				else
					sprintf(text,"%s:%s",buffer,row[4]);
				text2 = malloc(strlen(text) + strlen(text3)+5);
				sprintf(text2,"%s\n%s",text,text3);
				text3 = malloc(strlen(text2)+30);
				if(atoi(row[3]) == 1)
				{
					sprintf(text3,"\033[37m%s",text2);
					index++;
				}
				else
					sprintf(text3,"\033[0m%s",text2);
		
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			text = malloc(300);
			sprintf(text,"UPDATE message SET Seen = 1 WHERE Seen = 0 AND `From` = %d AND `To` = %d",id2,id1);
			query(con,text);
			break;
		case 5: //receive message
			read(thD.fd,&id1,sizeof(id1));
			read(thD.fd,&id2,sizeof(id2));
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			buffer2 = malloc(strlen(buffer));
			strcpy(buffer2,buffer);
			if(exitFromChatOccured(buffer2))
			{
				thD.option = 4;
				write(thD.fd,&thD.option,sizeof(thD.option));
				chatWithHasLeftConversation(id2);
			}
			else
			{
				thD.option = 10;
				write(thD.fd,&thD.option,sizeof(thD.option));
				sendMessageToUser(id1,id2,buffer);
			}
			break;
		case 6: //finish register
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			registerAcc(buffer,accounts,con);
			break;
		case 7://continue login
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			fflush(stdout);
			if(isOnline(con,buffer) != 0)
			{
				if(loginAcc(con,buffer) != 0)
				{
					bzero(&accounts[thD.fd],sizeof(accounts[thD.fd]));
					populateAccounts(con,thD.fd,buffer);
					thD.option = 4;
					write(thD.fd,&thD.option,sizeof(thD.option));
				}
				else
				{
					thD.option = 6;
					write(thD.fd,&thD.option,sizeof(thD.option));
				}
			}
			else
			{
				thD.option = 8;
				write(thD.fd,&thD.option,sizeof(thD.option));
				wAccesingOnlineAccount(buffer,nfds);
			}
			break;
		case 8: //actualizateNewMessages
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(300 + bufferSize);
			sprintf(text,"SELECT COUNT(ID) FROM message WHERE Seen = 0 AND `To` = (SELECT ID FROM users WHERE Name = '%s')",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
				bufferSize = atoi(row[0]);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			break;
		case 9: //show friends
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(bufferSize+300);
			sprintf(text,"SELECT Name,Status FROM users,friends WHERE users.ID = friends.Friend and friends.ID = (select ID from users where Name = '%s')",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			bzero(&text2,sizeof(text2));
			text3 = malloc(3);
			text3[0] = '>';
			text3[1] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d",row[0],atoi(row[1]));
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d,",row[0],atoi(row[1]));
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
		case 10: //remove friend
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize] = '\0';
			text = malloc(300+strlen(buffer)+bufferSize);
			sprintf(text,"SELECT 1 from friends WHERE ID = (SELECT ID FROM users WHERE Name = '%s') AND Friend = (SELECT ID FROM users WHERE Name = '%s')",buffer,buffer2);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
			{
				thD.option = 106;
				write(thD.fd,&thD.option,sizeof(thD.option));
				write(thD.fd,&bufferSize,sizeof(bufferSize));
				write(thD.fd,buffer2,bufferSize);
				text = malloc(300+strlen(buffer) + bufferSize);
				sprintf(text,"DELETE FROM friends WHERE ID = (SELECT ID FROM users WHERE Name = '%s') AND Friend = (SELECT ID FROM users WHERE Name = '%s')",buffer,buffer2);
				query(con,text);
			}
			else
			{
				thD.option = 105;
				write(thD.fd,&thD.option,sizeof(thD.option));
				write(thD.fd,&bufferSize,sizeof(bufferSize));
				write(thD.fd,buffer2,bufferSize);
			}
			break;
		case 11: //show all users
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(bufferSize+150);
			sprintf(text,"SELECT Name,Status FROM users WHERE Name != '%s'",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			bzero(&text2,sizeof(text2));
			text3 = malloc(3);
			text3[0] = '>';
			text3[1] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d",row[0],atoi(row[1]));
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d,",row[0],atoi(row[1]));
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
		case 12: // search users by name
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize-1] = '\0';
			text = malloc(bufferSize+250);
			text2 = malloc(3);
			text2[0] = '%';
			text2[1] = '\0';
			sprintf(text,"SELECT Name,Status FROM users WHERE Name != '%s' and Name like '%s%s%s'",buffer,text2,buffer2,text2);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			bzero(&text2,sizeof(text2));
			text3 = malloc(3);
			text3[0] = '>';
			text3[1] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d",row[0],atoi(row[1]));
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d,",row[0],atoi(row[1]));
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
		case 13: //check inbox
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(bufferSize+300);
			sprintf(text,"SELECT (SELECT Name FROM users WHERE ID = message.`From`),count(ID) FROM message WHERE Seen = 0 AND `To` = (SELECT ID FROM users WHERE Name = '%s') GROUP BY `To`,`From`",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			bzero(&text2,sizeof(text2));
			text3 = malloc(3);
			text3[0] = '>';
			text3[1] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d",row[0],atoi(row[1]));
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%d,",row[0],atoi(row[1]));
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
		case 14://show all chatrooms
			text = malloc(350);
			sprintf(text,"SELECT DISTINCT Name,Password FROM chatRooms");
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			text3 = malloc(3);
			text3[0] = '>';
			text3[1] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%s",row[0],row[1]);
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%s,",row[0],row[1]);
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
		case 15:
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			text = malloc(300 + bufferSize);
			sprintf(text,"SELECT 1 FROM chatRooms WHERE Name = '%s'",buffer);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
			{
				thD.option = 16;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			else
			{
				thD.option = 23;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			break;
		case 16://create chatroom
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text = malloc(bufferSize);
			read(thD.fd,text,bufferSize);
			text[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text2 = malloc(bufferSize);
			read(thD.fd,text2,bufferSize);
			text2[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text3 = malloc(bufferSize);
			read(thD.fd,text3,bufferSize);
			text3[bufferSize] = '\0';
			buffer = malloc(strlen(text)+strlen(text2)+bufferSize+300);
			sprintf(buffer,"INSERT INTO chatRooms VALUES(0,'%s',(SELECT ID FROM users WHERE Name = '%s'),'%s')",text2,text,text3);
			query(con,buffer);
			thD.option = 24;
			write(thD.fd,&thD.option,sizeof(thD.option));
			break;
		case 17://exit from chatroom
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize] = '\0';
			text = malloc(3);
			text[0] = '0';
			text[1] = '\0';
			sendMessageToLobby(buffer,buffer2,text);
			buffer2 = malloc(300 + strlen(buffer));
			sprintf(buffer2,"DELETE FROM chatRooms WHERE User = (SELECT ID FROM users WHERE Name = '%s')",buffer);
			query(con,buffer2);
			break;
		case 18: //sendMessageToLobby
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text = malloc(bufferSize);
			read(thD.fd,text,bufferSize);
			text[bufferSize] = '\0';
			sendMessageToLobby(buffer,buffer2,text);
			break;
		case 19://enter in chatroom without password
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer = malloc(bufferSize);
			read(thD.fd,buffer,bufferSize);
			buffer[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize] = '\0';	
			text = malloc(strlen(buffer)+bufferSize+250);
			sprintf(text,"INSERT INTO chatRooms VALUES(0,'%s',(SELECT ID FROM users WHERE Name = '%s'),'0')",buffer2,buffer);
			query(con,text);
			break;
		case 20://login into chatroom
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text = malloc(bufferSize);
			read(thD.fd,text,bufferSize);
			text[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text2 = malloc(bufferSize);
			read(thD.fd,text2,bufferSize);
			text2[bufferSize] = '\0';
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text3 = malloc(bufferSize);
			read(thD.fd,text3,bufferSize);
			text3[bufferSize] = '\0';
			buffer = malloc(200+strlen(text2));
			sprintf(buffer,"SELECT DISTINCT 1 FROM chatRooms WHERE Name = '%s'",text2);
			query(con,buffer);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			if((row = mysql_fetch_row(result)))
			{
				buffer = malloc(250+strlen(text2)+strlen(text3));
				sprintf(buffer,"SELECT DISTINCT 1 FROM chatRooms WHERE Name = '%s' AND Password = '%s'",text2,text3);
				query(con,buffer);
				result = mysql_store_result(con);
				if(result == NULL)
					finish_with_error(con);
				if((row = mysql_fetch_row(result)))
				{
					buffer = malloc(strlen(text)+strlen(text2)+300);
					sprintf(buffer,"INSERT INTO chatRooms VALUES(0,'%s',(SELECT ID FROM users WHERE Name = '%s'),'%s')",text2,text,text3);
					query(con,buffer);
					thD.option = 24;
					write(thD.fd,&thD.option,sizeof(thD.option));
				}
				else
				{
					thD.option = 27;
					write(thD.fd,&thD.option,sizeof(thD.option));
				}
			}
			else
			{
				thD.option = 17;
				write(thD.fd,&thD.option,sizeof(thD.option));
			}
			break;
		case 21://see who is in chatroom
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			text = malloc(bufferSize);
			read(thD.fd,text,bufferSize);
			text[bufferSize] = '\0';
			text2 = malloc(strlen(text)+300);
			sprintf(text2,"SELECT Name FROM users WHERE EXISTS (SELECT User FROM chatRooms WHERE Name = '%s' AND User != -1 AND User = users.ID)",text);
			query(con,text2);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			buffer2 = malloc(2);
			buffer2[0] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				buffer2 = malloc(strlen(row[0]));
				strcpy(buffer2,row[0]);
			}
			while((row = mysql_fetch_row(result)))
			{
				buffer = malloc(strlen(row[0]) + strlen(buffer2)+5);
				sprintf(buffer,"%s,%s",buffer2,row[0]);
				buffer2 = malloc(strlen(buffer));
				strcpy(buffer2,buffer);
			} 
			bufferSize = strlen(buffer2);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,buffer2,bufferSize);
			break;
		case 22://search chatroom by name
			read(thD.fd,&bufferSize,sizeof(bufferSize));
			buffer2 = malloc(bufferSize);
			read(thD.fd,buffer2,bufferSize);
			buffer2[bufferSize-1] = '\0';
			text = malloc(bufferSize+250);
			text2 = malloc(3);
			text2[0] = '%';
			text2[1] = '\0';
			sprintf(text,"SELECT Name,Password FROM chatRooms WHERE Name like '%s%s%s'",text2,buffer2,text2);
			query(con,text);
			result = mysql_store_result(con);
			if(result == NULL)
				finish_with_error(con);
			bzero(&text2,sizeof(text2));
			text3 = malloc(3);
			text3[0] = '\0';
			if((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%s",row[0],row[1]);
				text3 = malloc(strlen(text));
				strcpy(text3,text);
				fflush(stdout);
			}
			while((row = mysql_fetch_row(result)))
			{
				text = malloc(strlen(row[0]) + 5);
				sprintf(text,"%s,%s,",row[0],row[1]);
				text2 = malloc(strlen(text) + strlen(text3));
				sprintf(text2,"%s,%s",text3,text);
				strcpy(text3,text2);
			}
			bufferSize = strlen(text3);
			write(thD.fd,&bufferSize,sizeof(bufferSize));
			write(thD.fd,text3,bufferSize);
			break;
	}
	FD_SET(thD.fd,&actfds);
	return (NULL);
}

