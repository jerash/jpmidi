/*
 *  tcpserver.c - TCP server interface for jpmidi
 *  Based on simple web examples
 *  Copyright (C) 2013 Raphaël Mouneyres
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h> /* isalpla */
#include "commands.h"
#include "cmdline.h"

/* includes for server mode */
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
 
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
#define CRLF		"\r\n"
#define MAX_CLIENTS 	100
#define BUF_SIZE	1024

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;
typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;

static void app(int tcp_port);
static int init_connection(int tcp_port);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);
static void send_message_to_all_clients(Client *clients, Client client, int actual, const char *buffer, char from_server);
static void remove_client(Client *clients, int to_remove, int *actual);
static void clear_clients(Client *clients, int actual);

/**** do_command()
 There is a separate instance of this function for each client connection.
 ****************/
void cleanup_string(char *buffer)
{
   char *cmd;
   int len = strlen(buffer);

   if (len > 0) {
	int cur = 0;
	while(cur <= len) {
		if(isalnum(buffer[cur])==0) {
			/*replace non alphanumeric character with space*/
			buffer[cur] = ' ';
		}
		cur++;
	}
	buffer[len+1]='\0';
   }

   /* Remove leading and trailing whitespace from the line. */
   cmd = stripwhite(buffer);
   strcpy(buffer,cmd);
}

unsigned char do_command (char *buffer)
{

   cleanup_string(buffer);
   /* If anything left, add to history and execute it. */
   if (*buffer) {
	/* execute command */
	printf("Received command: -- %s --\n",buffer);
	if(strcmp(buffer,"shutdown")==0) {
		return 1;
	}
	else {
		execute_command(buffer);
	}
   }
   return 0;
}

void send_ack(int sock)
{
  /* send ack */
   if ( (write(sock,"ack",3)) < 0) perror("ERROR writing to socket");
}

void tcpserver(int tcp_port)
{
  app(tcp_port);
}

/*------------------------------------------*/

static void app(int tcp_port)
{
   SOCKET sock = init_connection(tcp_port);
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];
   Client client;
   
   unsigned char running = 1; // server needs to run until exit

   fd_set rdfs;

   while(running)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

	      printf("New client connection on socket %d \n",csock);

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = { csock };
         strncpy(c.name, buffer, BUF_SIZE - 1);
         clients[actual] = c;
         actual++;
      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               client = clients[i];
               int c = read_client(clients[i].sock, buffer);
               if(c == 0)
               {
                  /* client disconnected */
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  // strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, "\n client disconnected !\n", BUF_SIZE - strlen(buffer) - 1);
                  printf("%s",buffer);
                  strcpy(buffer,"");
               }
               else
               {
                  /* DOSTUFF*/
		             printf("received buffer: %s \n",buffer);
		             if(do_command((char*)&buffer))
			            running=0;
               }
               break;
            }
         }
      }
   }

   send_message_to_all_clients(clients, client, actual, "jpmidi server shutdown", 0);
   printf("jpmidi server shutdown\n");
   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
   printf("Clients list cleared\n");
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != clients[i].sock)
      {
         if(from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static int init_connection(int tcp_port)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(tcp_port);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   printf("Server listening on port %d\n",tcp_port);

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
   printf("Socket %d closed\n",sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}
