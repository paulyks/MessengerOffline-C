#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#define clearScreen() printf("\033[H\033[J");fflush(stdout)
extern int errno;
int sd,isChatRoomProtected;
int port;
int writeFlag = 0,option=-1,bufferSize,readFlag = 0,newMessage = 0,checkID,isInChatRoom = 0;
char *buffer,*buffer2,*username,*chatWith;
int myID=-1,chatWithID=-1,isOnline = 0;
size_t sizeT = 500;
void onCtrlC(int sig)
{
  clearScreen();
  printf("\a");
  int option = -1;
  write(sd,&option,sizeof(option));
  bufferSize = strlen(username);
  write(sd,&isOnline,sizeof(isOnline));
  if(isOnline == 1)
  {
    write(sd,&chatWithID,sizeof(chatWithID));
    write(sd,&bufferSize,sizeof(bufferSize));
    write(sd,username,bufferSize);
    write(sd,&isInChatRoom,sizeof(isInChatRoom));
    if(isInChatRoom)
    {
      bufferSize = strlen(chatWith);
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,chatWith,bufferSize);
    }
    printf("Have a nice day, %s!\n",username);
    fflush(stdout);
  }
  printf("***Connection with Messenger by Paul has been finised.****\n");
  fflush(stdout);
  close(sd);
  exit(sd);
}
static void *writeToServer(void*);
int validUsername(char* name)
{
  int i=0;
  while(name[i] != '\0')
  {
    if( !((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i]  <= '9')) )
      return 0;
    i++;
  }
  return 1;
}
void actualizateNewMessages()
{
  int msg = 8;
  write(sd,&msg,sizeof(msg));
  bufferSize = strlen(username);
  readFlag = 1;
  write(sd,&bufferSize,sizeof(bufferSize));
  write(sd,username,bufferSize);
  read(sd,&newMessage,sizeof(newMessage));
  readFlag = 0;
}
int main (int argc, char *argv[])
{
  char *recvMessage;
  printf("Clientul %d a pornit.\n",getpid());
  fflush(stdout);
  signal(SIGINT,onCtrlC);
  struct sockaddr_in server;
  fd_set readfds;
  fd_set actfds;
  struct timeval tv;
  if(argc != 3)
  {
    printf("Sintax %s: <server_addr> <port>\n",argv[0]);
    return 0;
  }
  port = atoi(argv[2]);
  if((sd = socket(AF_INET,SOCK_STREAM,0)) == -1)
  {
    perror("Socket error!\n");
    return errno;
  }
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(port);

  if(connect(sd,(struct sockaddr*)&server,sizeof(struct sockaddr)) == -1)
  {
    perror("Connect error!\n");
    return errno;
  }
  printf("Welcome to Messenger!\n");
  fflush(stdout);
  printf("[0]Login.\n[1]Register.\n");
  FD_ZERO(&actfds);
  FD_SET(sd,&actfds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  pthread_t th;
  while(1)
  {
    bcopy((char*)&actfds,(char*)&readfds,sizeof(readfds));
    if(select(sd+1,&readfds,NULL,NULL,&tv) < 0)
    {
      perror("Select error!\n");
      return errno;
    }
    if(FD_ISSET(sd,&readfds))
    {
      if(readFlag == 0)
      {
        read(sd,&option,sizeof(option));
        switch(option)
        {
          case 100: //wAccesingOnlineAccount
            printf("***Your account was accesed from another location!***\n");
            fflush(stdout);
            break;
          case 101:
            printf(">%s is already in your friend list.\n",chatWith);
            fflush(stdout);
            option = 9;
            break;
          case 102:
            printf(">%s was added in your friend list.\n",chatWith);
            option = 4;
            break;
          case 103: //receive message
            read(sd,&checkID,sizeof(checkID));
            read(sd,&bufferSize,sizeof(bufferSize));
            bzero(&recvMessage,sizeof(recvMessage));
            recvMessage = malloc(bufferSize);
            read(sd,recvMessage,bufferSize);
            recvMessage[bufferSize] = '\0';
            if(checkID == chatWithID)
            {
              printf("%s\n");
              fflush(stdout);
            }
            else
            {
              recvMessage = strtok(recvMessage,":");
              printf("***You have a new message from %s.***\n",recvMessage);
              newMessage++;
              fflush(stdout);
            }
            free(recvMessage);
            break;
          case 104:
            if(chatWithID != -1)
            {
              printf(">%s has left conversation.\n",chatWith);
              fflush(stdout);
            }
            break;
          case 105: //is not your friend to remove it
            read(sd,&bufferSize,sizeof(bufferSize));
            recvMessage = malloc(bufferSize);
            read(sd,recvMessage,bufferSize);
            recvMessage[bufferSize] = '\0';
            printf("%s is not in your friend list.\n",recvMessage);
            fflush(stdout);
            break;
          case 106:
            read(sd,&bufferSize,sizeof(bufferSize));
            recvMessage = malloc(bufferSize);
            read(sd,recvMessage,bufferSize);
            recvMessage[bufferSize] = '\0';
            printf("%s is not your friend anymore.\n",recvMessage);
            fflush(stdout);
            break;
          case 107:
            printf("Already exists a chatroom with name %s.",chatWith);
            option = 18;
            break;
          case 108://receive message from lobby
            read(sd,&bufferSize,sizeof(bufferSize));
            recvMessage = malloc(bufferSize);
            read(sd,recvMessage,bufferSize);
            recvMessage[bufferSize] = '\0';
            printf("%s",recvMessage);
            fflush(stdout);
            break;
        }
      }
    }
    if(writeFlag == 0)
    {
      writeFlag = 1;
      pthread_create(&th,NULL,&writeToServer,NULL);
    }
    FD_CLR(sd,&readfds);
  }
  close(sd);
  return 0;
}
static void *writeToServer(void *arg)
{
  int i;
  char* text;
  switch(option)
  {
    case -1:
      buffer = malloc(1);
      bufferSize = 1;
      read(0,buffer,1);
      clearScreen();
      if(buffer[0] == '0')
        option = 0; // login
      else
      {
        option = 1; // register
        printf("Register on Messenger Platform by Paul.\n");
      }
      break;
    case 0: //login
      clearScreen();
      printf(">Login into Messenger by Paul.\n");
      fflush(stdout);
      printf("Username:");
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)-1] = '\0';
      username = malloc(strlen(buffer));
      strcpy(username,buffer);
      if(buffer[0] != '\0')
      {
        write(sd,&option,sizeof(option));
        bufferSize = strlen(buffer);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        option = -100;
      }
      else
        option = 0;
      break;
    case 1: // register
      printf("Username:");
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)-1] = '\0';
      username = malloc(strlen(buffer));
      strcpy(username,buffer);
      if(buffer[0] == '\0')
      {
        clearScreen();
        printf(">Register on Messenger Platform by Paul.\n");
        fflush(stdout);
        break;
      }
      if(validUsername(buffer))
      {
        if(strlen(buffer) > 4)
        {
          bufferSize = strlen(buffer);
          write(sd,&option,sizeof(option));
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,buffer,bufferSize);
          option = -100;
        }
        else
        {
          clearScreen();
          printf(">Register on Messenger Platform by Paul.\n");
          fflush(stdout);
          printf("%s is too short for an username.\n",buffer);
          fflush(stdout);
        }
      }
      else
      {
        clearScreen();
        printf(">Register on Messenger Platform by Paul.\n");
        fflush(stdout);
        printf("An username can contain only alphabet symbols and numbers.\n");
        fflush(stdout);
      }
      break;
    case 2://error register
      printf("%s already exist in database.\n",buffer);
      option = 1;
      break;
    case 3:
      printf("Password:");
      fflush(stdout);
      buffer2 = malloc(500);
      getline(&buffer2,&sizeT,stdin);
      buffer2[strlen(buffer2)-1] = '\0';
      if(buffer2[0] == '\0')
      {
        break;
      }
      if(strlen(buffer2) > 4)
      {
        option = 6;
        write(sd,&option,sizeof(option));
        sprintf(buffer,"%s:%s",buffer,buffer2);
        bufferSize = strlen(buffer);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        option = 4;
      }
      else
      {
        option = 3;
        printf("Your password is too weak.\n");
        fflush(stdout);
      }
      break;//request password
    case 4: //general menu
      isOnline = 1;
      bzero(&chatWith,sizeof(chatWith));
      chatWithID = -1;
      actualizateNewMessages();
      clearScreen();
      printf(">Messenger by Paul\n");
      printf("----You: %s----\n",username);
      printf("|[1]----View friends----|\n");
      printf("|[2]----View online users----|\n");
      printf("|[3]----View all users----|\n");
      printf("|[4]----Search users by name----|\n");
      printf("|[5]----Check inbox(%d new messages)----|\n",newMessage);
      printf("|[6]----Chatrooms----|\n");
      fflush(stdout);
      buffer = malloc(1);
      scanf("%s",buffer);
      switch(buffer[0])
      {
        case '1':option=11;break;
        case '2':option=12;break;
        case '3':option=13;break;
        case '4':option=14;break;
        case '5':option=15;break;
        case '6':option=16;break;
        default:option=4;break;
      }
      clearScreen();
      break;
    case 5: //continue login
      printf("Password:");
      fflush(stdout);
      buffer2 = malloc(256);
      scanf("%s",buffer2);
      clearScreen();
      option = 7;
      write(sd,&option,sizeof(option));
      username = malloc(strlen(buffer));
      sprintf(username,"%s",buffer);
      username[strlen(username)] = '\0';
      sprintf(buffer,"%s:%s",buffer,buffer2);
      bufferSize = strlen(buffer);
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,buffer,bufferSize);
      strcpy(buffer,username);
      option = -100;
      break;
    case 6:
      option = 0;
      write(sd,&option,sizeof(option));
      option = -100;
      bufferSize = strlen(username);
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,username,bufferSize);
      clearScreen();
      printf(">Login into Messenger by Paul.\n");
      fflush(stdout);
      printf("***Incorrect password, try again.***\n");
      fflush(stdout);
      printf("Username:%s\n",username);
      fflush(stdout);
      break;//incorrect password
    case 7:
      option = 0;
      clearScreen();
      printf(">Login into Messenger by Paul.\n");
      fflush(stdout);
      printf("***This username doesn't exist in database.***\n");
      fflush(stdout);
      break;//error login
    case 8:
      clearScreen();
      printf(">Login into Messenger by Paul.\n");
      fflush(stdout);
      printf("***%s is already online. Are you sure this is your account?***\n",username);
      fflush(stdout);
      option = 0;
      break;//error login2
    case 9: //menu with user
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      printf(">>[1] Open chat with %s.\n",chatWith);
      fflush(stdout);
      printf(">>[2] Add %s in your friend list.\n",chatWith);
      fflush(stdout);
      printf(">>[3] Remove %s from your friend list.\n",chatWith);
      fflush(stdout);
      printf("[0] Back\n");
      fflush(stdout);
      buffer = malloc(1);
      scanf("%s",buffer);
      clearScreen();
      switch(buffer[0])
      {
        case '0':
          option = 4;
          break;
        case '1':
          option = 4;
          write(sd,&option,sizeof(option));
          bufferSize = strlen(username);
          readFlag = 1;
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          read(sd,&myID,sizeof(myID));
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          read(sd,&chatWithID,sizeof(chatWithID));
          read(sd,&bufferSize,sizeof(bufferSize));
          read(sd,buffer,bufferSize);
          buffer[bufferSize] = '\0';
          printf("%s\n",buffer);
          buffer[0] = '\0';
          readFlag = 0;
          option = 10;
          break;
        case '2':
          option = 3;
          write(sd,&option,sizeof(option));
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          option = 9;
          break;
        case '3':
          option = 10;
          write(sd,&option,sizeof(option));
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          option = 9;
          break;
        default:
          option = 9;
          break;
        }
      break; 
    case 10:
      fflush(stdout);
      bzero(&buffer2,sizeof(buffer2));
      bzero(&buffer,sizeof(buffer));
      buffer2 = malloc(500);
      getline(&buffer2,&sizeT,stdin);
      option = 10;
      if(buffer2[0] != '\n')
      {
        buffer = malloc(strlen(username) + strlen(buffer2) + 5);
        sprintf(buffer,"%s:%s",username,buffer2);
        fflush(stdout);
        bufferSize = strlen(buffer);  
        option = 5;
        write(sd,&option,sizeof(option));
        write(sd,&myID,sizeof(myID));
        write(sd,&chatWithID,sizeof(chatWithID));
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        option = -100;
      }
      break;//sendChatMessage
    case 11: // view friends
      option = 9;
      write(sd,&option,sizeof(option));
      bufferSize = strlen(username);
      readFlag = 1;
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,username,bufferSize);
      read(sd,&bufferSize,sizeof(bufferSize));
      buffer = malloc(bufferSize);
      read(sd,buffer,bufferSize);
      buffer[bufferSize] = '\0';
      readFlag = 0;
      i = 1;
      strcpy(buffer2,buffer);
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      if(bufferSize > 4)
      {
        buffer = strtok(buffer,",");
        printf(">[%d]%s",i++,buffer);
        buffer = strtok(NULL,",");
        if(buffer[0] == '0')
          printf("(OFF)\n");
        else
          printf("(ON)\n");
        fflush(stdout);
        buffer = strtok(NULL,",");
        while(buffer)
        {
          printf(">[%d]%s",i++,buffer);
          fflush(stdout);
          buffer = strtok(NULL,",");
          if(buffer[0] == '0')
            printf("(OFF)\n");
          else
            printf("(ON)\n");
          fflush(stdout);
          buffer = strtok(NULL,",");
        }
      }
      else
      {
        printf("Your friend list is empty.\n");
        fflush(stdout);
      }
      printf("[0]Back\n");
      fflush(stdout);
      buffer = malloc(3);
      scanf("%s",buffer);
      if(atoi(buffer) > 0 && atoi(buffer) < i)
      {
        option = atoi(buffer);
        buffer2 = strtok(buffer2,",");
        option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          buffer2 = strtok(NULL,",");
          option--;
        }
        chatWith = malloc(strlen(buffer));
        strcpy(chatWith,buffer2);
        option = 9;
      } 
      else
        option = 4; 
      break;
    case 12: //show online users
      option = 2;
      write(sd,&option,sizeof(option));
      readFlag = 1;
      read(sd,&bufferSize,sizeof(bufferSize));
      buffer = malloc(bufferSize+5);
      read(sd,buffer,bufferSize);
      readFlag = 0;
      buffer[bufferSize] = '\0';
      buffer2 = malloc(strlen(buffer)+1);
      strcpy(buffer2,buffer);
      fflush(stdout);
      i = 1;
      buffer = strtok(buffer,",");
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      if(buffer)
      {
         if(strcmp(username,buffer) != 0)
         {
          printf(">[%d]%s\n",i++,buffer);
          fflush(stdout);
        }
        buffer = strtok(NULL,",");
        while(buffer)
        {
          if(strcmp(username,buffer) != 0)
          {
            printf(">[%d]%s\n",i++,buffer);
            fflush(stdout);
          }
          buffer = strtok(NULL,",");
        }
      }
      if(i == 1)
      {
        printf("There are no users online.\n");
        fflush(stdout);
      }
      printf("[0]Back\n");
      fflush(stdout);
      buffer = malloc(20);
      scanf("%s",buffer);
      clearScreen();
      option = atoi(buffer);
      if(option == 0)
          option = 4;
      if(option > 0 && option <= i)
      {
        buffer2 = strtok(buffer2,",");
        if(strcmp(username,buffer2) != 0)
          option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          if(strcmp(username,buffer2) != 0)
            option--;
        }
        chatWith = malloc(strlen(buffer2));
        strcpy(chatWith,buffer2);
        option = 9;
      }
      else
        option = 4;
      break;
    case 13: // view all users
      option = 11;
      write(sd,&option,sizeof(option));
      bufferSize = strlen(username);
      readFlag = 1;
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,username,bufferSize);
      read(sd,&bufferSize,sizeof(bufferSize));
      buffer = malloc(bufferSize);
      read(sd,buffer,bufferSize);
      buffer[bufferSize] = '\0';
      readFlag = 0;
      i = 1;
      strcpy(buffer2,buffer);
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      if(bufferSize > 4)
      {
        buffer = strtok(buffer,",");
        printf(">[%d]%s",i++,buffer);
        buffer = strtok(NULL,",");
        if(buffer[0] == '0')
          printf("(OFF)\n");
        else
          printf("(ON)\n");
        fflush(stdout);
        buffer = strtok(NULL,",");
        while(buffer)
        {
          printf(">[%d]%s",i++,buffer);
          fflush(stdout);
          buffer = strtok(NULL,",");
          if(buffer[0] == '0')
            printf("(OFF)\n");
          else
            printf("(ON)\n");
          fflush(stdout);
          buffer = strtok(NULL,",");
        }
      }
      else
      {
        printf("There are no users except you.\n");
        fflush(stdout);
      }
      printf("[0]Back\n");
      fflush(stdout);
      buffer = malloc(3);
      scanf("%s",buffer);
      if(atoi(buffer) > 0 && atoi(buffer) < i)
      {
        option = atoi(buffer);
        buffer2 = strtok(buffer2,",");
        option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          buffer2 = strtok(NULL,",");
          option--;
        }
        chatWith = malloc(strlen(buffer));
        strcpy(chatWith,buffer2);
        option = 9;
      } 
      else
        option = 4; 
      break;
    case 14: // search users by name
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n>Search users by name:",username);
      fflush(stdout);
      getline(&buffer,&sizeT,stdin);
      if(buffer[0] != '\n')
      {
        option = 12;
        write(sd,&option,sizeof(option));
        bufferSize = strlen(username);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,username,bufferSize);
        bufferSize = strlen(buffer);
        readFlag = 1;
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        read(sd,&bufferSize,sizeof(bufferSize));
        buffer = malloc(bufferSize);
        read(sd,buffer,bufferSize);
        buffer[bufferSize] = '\0';
        readFlag = 0;
        i = 1;
        buffer2 = malloc(strlen(buffer));
        strcpy(buffer2,buffer);
        if(bufferSize > 4)
        {
          buffer = strtok(buffer,",");
          printf(">[%d]%s",i++,buffer);
          buffer = strtok(NULL,",");
          if(buffer[0] == '0')
            printf("(OFF)\n");
          else
            printf("(ON)\n");
          fflush(stdout);
          buffer = strtok(NULL,",");
          while(buffer)
          {
            printf(">[%d]%s",i++,buffer);
            fflush(stdout);
            buffer = strtok(NULL,",");
            if(buffer[0] == '0')
              printf("(OFF)\n");
            else
              printf("(ON)\n");
            fflush(stdout);
            buffer = strtok(NULL,",");
          }
        }
        else
        {
          printf("There are no users to list.\n");
          fflush(stdout);
        }
        printf("[0]Back\n");
        fflush(stdout);
        buffer = malloc(3);
        scanf("%s",buffer);
        if(atoi(buffer) > 0 && atoi(buffer) < i)
        {
          option = atoi(buffer);
          buffer2 = strtok(buffer2,",");
          option--;
          while(option != 0)
          {
            buffer2 = strtok(NULL,",");
            buffer2 = strtok(NULL,",");
            option--;
          }
          chatWith = malloc(strlen(buffer));
          strcpy(chatWith,buffer2);
          option = 9;
        } 
        else
          option = 4; 
      }
      break;
    case 15://check inbox
      option = 13;
      write(sd,&option,sizeof(option));
      bufferSize = strlen(username);
      readFlag = 1;
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,username,bufferSize);
      read(sd,&bufferSize,sizeof(bufferSize));
      buffer = malloc(bufferSize);
      read(sd,buffer,bufferSize);
      buffer[bufferSize] = '\0';
      readFlag = 0;
      i = 1;
      strcpy(buffer2,buffer);
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      if(bufferSize > 4)
      {
        buffer = strtok(buffer,",");
        text = strtok(NULL,",");
        printf(">[%d]%s(%s)\n",i++,buffer,text);
        fflush(stdout);
        buffer = strtok(NULL,",");
        while(buffer)
        {
          text = strtok(NULL,",");
          printf(">[%d]%s(%s)\n",i++,buffer,text);
          fflush(stdout);
          buffer = strtok(NULL,",");
        }
      }
      else
      {
        printf("There are no new messages.\n");
        fflush(stdout);
      }
      printf("[0]Back\n");
      fflush(stdout);
      buffer = malloc(3);
      scanf("%s",buffer);
      if(atoi(buffer) > 0 && atoi(buffer) < i)
      {
        option = atoi(buffer);
        buffer2 = strtok(buffer2,",");
        option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          buffer2 = strtok(NULL,",");
          option--;
        }
        chatWith = malloc(strlen(buffer));
        strcpy(chatWith,buffer2);
        option = 9;
      } 
      else
        option = 4; 

      break;
    case 16: //enter in chatroom menu
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      printf(">[1] Show all chatrooms.\n");
      printf(">[2] Create chatroom.\n");
      printf(">[3] Search chatroom by name.\n");
      printf("[0] Back\n");
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      switch(buffer[0])
      {
        case '0':option=4;break;
        case '1':option=17;break;
        case '2':option=18;break;
        case '3':option=19;break;
        default:option=16;break;
      }
      break;
    case 17://show all chatrooms
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      readFlag = 1;
      option = 14;
      write(sd,&option,sizeof(option));
      read(sd,&bufferSize,sizeof(bufferSize));
      read(sd,buffer,bufferSize);
      buffer[bufferSize] = '\0';
      readFlag = 0;
      i = 1;
      buffer2 = malloc(strlen(buffer));
      strcpy(buffer2,buffer);
      if(bufferSize > 3)
      {
        buffer = strtok(buffer,",");
        printf(">[%d]%s",i++,buffer);
        buffer = strtok(NULL,",");
        if(buffer[0] != '0')
        {
          printf("*");
          fflush(stdout);
        }
        printf("\n");
        fflush(stdout);
        buffer = strtok(NULL,",");
        while(buffer)
        {
          printf(">[%d]%s",i++,buffer);
          fflush(stdout);
          buffer = strtok(NULL,",");
          if(buffer[0] != '0')
          {
            printf("*");
            fflush(stdout);
          }
          printf("\n");
          fflush(stdout);
          buffer = strtok(NULL,",");
        }
      }
      else
      {
        printf("There are no chat rooms online.\n");
        fflush(stdout);
      }
      printf("[0] Back\n");
      fflush(stdout);
      buffer = malloc(500);
      scanf("%s",buffer);
      if(atoi(buffer) > 0 && atoi(buffer) < i)
      {
        option = atoi(buffer);
        buffer2 = strtok(buffer2,",");
        option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          buffer2 = strtok(NULL,",");
          option--;
        }
        chatWith = malloc(strlen(buffer));
        strcpy(chatWith,buffer2);
        buffer2 = strtok(NULL,",");
        if(buffer2[0] == '0')
          isChatRoomProtected = 0;
        else
          isChatRoomProtected = 1;
        option = 20;
      } 
      else
        option = 16; 
      break;
    case 18: //create username for chatroom
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      printf("Chatroom name:");
      fflush(stdout);
      chatWith = malloc(500);
      getline(&chatWith,&sizeT,stdin);
      chatWith[strlen(chatWith)-1] = '\0';
      if(chatWith[0] !=  '\n')
      {
        if(validUsername(chatWith))
        {
          option = 15;
          write(sd,&option,sizeof(option));
          option = -100;
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
        }
      }
      break;
    case 19: //search chatroom by name
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n>Search chatrooms by name:",username);
      fflush(stdout);
      getline(&buffer,&sizeT,stdin);
      if(buffer[0] != '\n')
      {
        option = 22;
        write(sd,&option,sizeof(option));
        bufferSize = strlen(buffer);
        readFlag = 1;
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        read(sd,&bufferSize,sizeof(bufferSize));
        buffer = malloc(bufferSize);
        read(sd,buffer,bufferSize);
        buffer[bufferSize] = '\0';
        readFlag = 0;
        i = 1;
        buffer2 = malloc(strlen(buffer));
        strcpy(buffer2,buffer);
        if(bufferSize > 1)
        {
          buffer = strtok(buffer,",");
          printf(">[%d]%s",i++,buffer);
          buffer = strtok(NULL,",");
          if(buffer[0] != '0')
          {
            printf("*");
            fflush(stdout);
          }
          printf("\n");
          fflush(stdout);
          buffer = strtok(NULL,",");
          while(buffer)
          {
            printf(">[%d]%s",i++,buffer);
            fflush(stdout);
            buffer = strtok(NULL,",");
            if(buffer[0] != '0')
            {
              printf("*");
              fflush(stdout);
            }
            printf("\n");
            fflush(stdout);
            buffer = strtok(NULL,",");
          }
        }
        else
        {
          printf("There are no chat rooms online.\n");
          fflush(stdout);
        }
        printf("[0] Back\n");
        fflush(stdout);
        buffer = malloc(500);
        scanf("%s",buffer);
        if(atoi(buffer) > 0 && atoi(buffer) < i)
        {
          option = atoi(buffer);
          buffer2 = strtok(buffer2,",");
          option--;
          while(option != 0)
          {
            buffer2 = strtok(NULL,",");
            buffer2 = strtok(NULL,",");
            option--;
          }
          chatWith = malloc(strlen(buffer));
          strcpy(chatWith,buffer2);
          buffer2 = strtok(NULL,",");
          if(buffer2[0] == '0')
            isChatRoomProtected = 0;
          else
            isChatRoomProtected = 1;
          option = 20;
        } 
        else
          option = 16; 
      }
      break;
    case 20: //menu with chatroom
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      printf(">[1] Enter in chatroom %s.\n",chatWith);
      printf(">[2] See who is in chatroom %s.\n",chatWith);
      printf("[0] Back\n");
      fflush(stdout);
      buffer = malloc(50);
      scanf("%s",buffer);
      switch(buffer[0])
      {
        case '0':option=16;break;
        case '1':option=21;break;
        case '2':option=22;break;
        default:option=20;break;
      }
      break;
    case 21://enter in chatroom
      if(isChatRoomProtected)
        option = 26;
      else
      {
        option = 19;
        write(sd,&option,sizeof(option));
        bufferSize = strlen(username);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,username,bufferSize);
        bufferSize = strlen(chatWith);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,chatWith,bufferSize);
        option = 24;
      }
      break;
    case 22://see who is in chatroom
      clearScreen();
      option = 21;
      write(sd,&option,sizeof(option));
      readFlag = 1;
      bufferSize = strlen(chatWith);
      write(sd,&bufferSize,sizeof(bufferSize));
      write(sd,chatWith,bufferSize);
      read(sd,&bufferSize,sizeof(bufferSize));
      buffer = malloc(bufferSize+5);
      read(sd,buffer,bufferSize);
      readFlag = 0;
      buffer[bufferSize] = '\0';
      buffer2 = malloc(strlen(buffer)+1);
      strcpy(buffer2,buffer);
      i = 1;
      buffer = strtok(buffer,",");
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\nChatroom:%s\n>Online users\n",username,chatWith);
      fflush(stdout);
      if(buffer)
      {
         if(strcmp(username,buffer) != 0)
         {
          printf(">[%d]%s\n",i++,buffer);
          fflush(stdout);
        }
        buffer = strtok(NULL,",");
        while(buffer)
        {
          if(strcmp(username,buffer) != 0)
          {
            printf(">[%d]%s\n",i++,buffer);
            fflush(stdout);
          }
          buffer = strtok(NULL,",");
        }
      }
      if(i == 1)
      {
        printf("There are no users online in %s.\n",chatWith);
        fflush(stdout);
      }
      printf("[0]Back\n");
      fflush(stdout);
      buffer = malloc(20);
      scanf("%s",buffer);
      clearScreen();
      option = atoi(buffer);
      if(option == 0)
          option = 16;
      if(option > 0 && option <= i)
      {
        buffer2 = strtok(buffer2,",");
        if(strcmp(username,buffer2) != 0)
          option--;
        while(option != 0)
        {
          buffer2 = strtok(NULL,",");
          if(strcmp(username,buffer2) != 0)
            option--;
        }
        chatWith = malloc(strlen(buffer2));
        strcpy(chatWith,buffer2);
        option = 9;
      }
      else
        option = 17;
      break;
    case 23://create password for chatroom
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      printf("Chatroom name:%s\n",chatWith);
      fflush(stdout);
      printf("Chatroom password(use 0 for no password):");
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)-1] = '\0';
      if(buffer[0] != '\n')
      {
        option = 16;
        write(sd,&option,sizeof(option));
        bufferSize = strlen(username);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,username,bufferSize);
        bufferSize = strlen(chatWith);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,chatWith,bufferSize);
        bufferSize = strlen(buffer);
        option = -100;
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
      }
      break;
    case 24://open chatroom
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n",username);
      fflush(stdout);
      printf("Chatroom name:%s\n>Use 0 to exit from chatroom.\n",chatWith);
      fflush(stdout);
      option = 25;
      break;
    case 25://talk in chatroom
      if(isInChatRoom == 0)
      {
        buffer = malloc(2);
        buffer[0] = '\0';
        option = 18;
        write(sd,&option,sizeof(option));
        option = 25;
        bufferSize = strlen(username);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,username,bufferSize);
        bufferSize = strlen(chatWith);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,chatWith,bufferSize);
        bufferSize = strlen(buffer);
        write(sd,&bufferSize,sizeof(bufferSize));
        write(sd,buffer,bufferSize);
        isInChatRoom = 1;
      }
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)] = '\0';
      if(buffer[0] != '\n')
      {
        if(buffer[0] == '0' && buffer[1] == '\n')
        {
          option = 17;
          write(sd,&option,sizeof(option));
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          bzero(&chatWith,sizeof(chatWith));
          isInChatRoom = 0;
          option = 16;
        }
        else
        {
          option = 18;
          write(sd,&option,sizeof(option));
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          bufferSize = strlen(buffer);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,buffer,bufferSize);
          option = 25;
        }
      }
      break;
    case 26://request password for entering chatroom
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n>Use 0 to get back.\n",username);
      fflush(stdout);
      printf("Chatroom name:%s\nPassword:",chatWith);
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)-1] = '\0';
      if(strlen(buffer) > 0)
      {
        if(buffer[0] != '0')
        {
          option = 20;
          write(sd,&option,sizeof(option));
          option = -100;
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          bufferSize = strlen(buffer);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,buffer,bufferSize);
        }
        else
          option = 17;
      }
      break;
    case 27://login error in chatRoom;
      clearScreen();
      printf(">Messenger by Paul.\n");
      fflush(stdout);
      printf("----You: %s----\n>Use 0 to get back.\n",username);
      fflush(stdout);
      printf("Chatroom name:%s\n%s is not the right password.\nTry again:",chatWith,buffer);
      fflush(stdout);
      buffer = malloc(500);
      getline(&buffer,&sizeT,stdin);
      buffer[strlen(buffer)-1] = '\0';
      if(strlen(buffer) > 0)
      {
        if(buffer[0] != '0')
        {
          option = 20;
          write(sd,&option,sizeof(option));
          option = -100;
          bufferSize = strlen(username);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,username,bufferSize);
          bufferSize = strlen(chatWith);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,chatWith,bufferSize);
          bufferSize = strlen(buffer);
          write(sd,&bufferSize,sizeof(bufferSize));
          write(sd,buffer,bufferSize);
        }
        else
          option = 17;
      }
      break;
  }
  
  writeFlag = 0;
  return(NULL);
}