#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)
char buffer2[BUFFER_SIZE];

//Variables
static int sd;
static ClientMessage_t *client;
static ServerMessage_t *server;
static struct sockaddr_in addr, addr2;
static struct timeval timeout;
static fd_set timeset;
char *tmmmp;


//Initialize the connection
int MFS_Init(char *hostname, int port){
  //Variables
  int rc;

  //Check for problems
  if(hostname == NULL || port == 0)
    return -1;

  //Open the port
  sd = UDP_Open(0);
  assert(sd > -1);

  //Fill the socket
  rc = UDP_FillSockAddr(&addr, hostname, port);
  assert(rc == 0);

  //Initialize the timeout
  FD_ZERO(&timeset);
  FD_SET(sd, &timeset);

  //Initialize timeout structure
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;

  return 0;
}

int MFS_Lookup(int pinum, char *name){
  //Variables
  int rc;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));
  
  //Setup the client message
  client->syscallNumber = LOOKUP;
  client->inum = pinum;
  strcpy(client->buffer, name);

  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  //Inode number is stored in returnCode
  return server->returnCode;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
  //Variables
  int rc, i;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));

  //Setup the client message
  client->syscallNumber = STAT;
  client->inum = inum;
  
  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  //Copy information to m
  m->type = server->type;
  m->size = server->size;
  m->blocksUsed = server->blocksUsed;
  for(i = 0; i < 14; i++)
    m->blocks[i] = server->blocks[i];

  return server->returnCode;
}

int MFS_Write(int inum, char *buffer, int block){
  //Variables
  int rc;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));

  //Setup the client message
  client->syscallNumber = WRITE;
  printf("client write: %d\n", inum);
  client->inum = inum;
  client->block = block;
  memcpy(client->buffer, buffer, MFS_BLOCK_SIZE);
  tmmmp = buffer;
  printf("client client write:%s", buffer);
  printf("mine\n");

  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  return server->returnCode;
}

int MFS_Read(int inum, char *buffer, int block){
  //Variables
  int rc;
  MFS_DirEnt_t *dir;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));

  //Setup the client message
  client->syscallNumber = READ;
  client->inum = inum;
  client->block = block;

  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  printf("client client buffer:%s", server->buffer);
  printf("Check\n");

  //Return correct info
  if(server->type == MFS_DIRECTORY){
    dir = (MFS_DirEnt_t *)malloc(sizeof(MFS_DirEnt_t));
    dir->inum = server->blocksUsed;
    memcpy(dir->name, server->buffer, 28);
    buffer = (char *)dir;
  }
  else{
    memcpy(buffer, server->buffer, MFS_BLOCK_SIZE);
    if(strcmp(buffer, tmmmp) == 0)
      printf("WE ARE THE SAME\n");
  }
  return server->returnCode;
}

int MFS_Creat(int pinum, int type, char *name){
  //Variables
  int rc;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));

  //Setup the client message
  client->syscallNumber = CREAT;
  client->inum = pinum;
  client->type = type;
  strcpy(client->buffer, name);

  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  return server->returnCode;
}

int MFS_Unlink(int pinum, char *name){
  //Variables
  int rc;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));
  
  //Setup the client message
  client->syscallNumber = UNLINK;
  client->inum = pinum;
  strcpy(client->buffer, name);
  
  //Send message to the server
  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));
  printf("Unlink: %d\n", server->returnCode);
  return server->returnCode;
}

int MFS_Shutdown(){
  //Variables
  int rc;
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));
  
  //Setup the client message
  client->syscallNumber = SHUTDOWN;

  rc = UDP_Write(sd, &addr, (char *)client, sizeof(ClientMessage_t));
  if(rc > 0)
    rc = UDP_Read(sd, &addr2, (char *)server, sizeof(ServerMessage_t));

  return server->returnCode;
}


