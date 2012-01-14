#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//Prototypes
int MyLookup(int pinum, char *name);
int MyStat(int inum);
int MyWrite(int inum, char *buffer, int block);
int MyRead(int inum, int block);
int MyCreat(int pinum, int type, char *name);
int MyUnlink(int pinum, char *name);
int createFile(int pinum, char *name);
int addDirectory(int pinum, char *name);
int addDirectoryEntry(int inum, char *name);
int findFreeInode();
int findMapPiece(int inum);

//Global Variables
void *image, *end, *position, *temp;
MFS_Checkpoint_t *checkpoint;
MFS_InodeMap_t *inodeMap[256];
MFS_Inode_t *inodes[4096];
int imageSize;
static ClientMessage_t *client;
static ServerMessage_t *server;

int
main(int argc, char *argv[]){
  //Variables
  int sd, fd, rc, returnCode, i, size;
  struct sockaddr_in s;
  int problem = sizeof(MFS_Dir_t) + sizeof(MFS_Inode_t) + 
    sizeof(MFS_InodeMap_t) + (2 * sizeof(MFS_DirEnt_t));

  //Initialize the client and server messages
  client = (ClientMessage_t *)malloc(sizeof(ClientMessage_t));
  server = (ServerMessage_t *)malloc(sizeof(ServerMessage_t));

  //Check for file image
  /*
  if(argc == 3){
    sd = UDP_Open(atoi(argv[1]));
    fd = open(argv[2], O_RDWR);
    assert(sd > -1);
    assert(fd > -1);
  }
  */
  if(argc == 3){
    printf("Check: %d\n", atoi(argv[1]));
    //Open the socket and the image
    sd = UDP_Open(atoi(argv[1]));
    //sd = UDP_Open(29344);
    fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    assert(sd > -1);
    assert(fd > -1);
    
    //Create image
    imageSize = sizeof(MFS_Checkpoint_t) + (256 * sizeof(MFS_InodeMap_t)) + 
      (4096 * sizeof(MFS_Inode_t)) + (14 * 4096 * 4096);
    image = malloc(imageSize);
    if(image == NULL)
      printf("MALLOC PROBLEM\n");
    size = 0;

    //Create and initialize the checkpoint
    temp = image;
    position = image;
    end = image;
    checkpoint = (MFS_Checkpoint_t *)temp;
    position += sizeof(MFS_Checkpoint_t);

    //Initialize the arrays
    for(i = 0; i < 256; i++)
      inodeMap[i] = NULL;
    for(i = 0; i < 4096; i++)
      inodes[i] = NULL;

    //Create the root directory
    rc = addDirectory(0, "/");
  }
  else{
    printf("Usage: server portnum [file-system-image]\n");
    exit(-1);
  }

  //Start server loop
  while(1){
    //Check to make sure size is okay
    

    //Get the message from the Client
    rc = UDP_Read(sd, &s, (char *)client, sizeof(ClientMessage_t));
    
    //Perform the correct operation
    if(rc > 0){
      if(client->syscallNumber == LOOKUP){
	//Lookup the file
	returnCode = MyLookup(client->inum, client->buffer);
	printf("RETURNCODE: %d\n", returnCode);
	//Setup the return message
	server->returnCode = returnCode;
	printf("server server return: %d\n", server->returnCode);
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else if(client->syscallNumber == STAT){
	//Get the Stat info about the specific inum
	returnCode = MyStat(client->inum);

	//Setup the return message
	server->returnCode = returnCode;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else if(client->syscallNumber == WRITE){
	printf("ENTERED WRITE 1\n");
	printf("here here here: %d\n", client->inum);
	//Update image
	returnCode = MyWrite(client->inum, client->buffer, client->block);
	
	//Update checkpoint
	checkpoint->end = end - image;

	//Write to disk
	rc = write(fd, image, imageSize);
	if(rc <= 0)
	  exit(-1);
	fsync(fd);

	//Setup the return message
	server->returnCode = returnCode;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else if(client->syscallNumber == READ){
	//Read the specified block of data
	returnCode = MyRead(client->inum, client->block);
        printf("server server server buffer: %s\n", server->buffer);
	//Setup the return message
	server->returnCode = returnCode;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else if(client->syscallNumber == CREAT){
	//Update image
	returnCode = MyCreat(client->inum, client->type, client->buffer);

	//Update checkpoint
	checkpoint->end = end - image;

	//Write to disk
	rc = write(fd, image, imageSize);
	if(rc <= 0)
	  exit(-1);
	fsync(fd);

	//Setup the return message
	server->returnCode = returnCode;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else if(client->syscallNumber == UNLINK){
	//Unlink and update message
	returnCode = MyUnlink(client->inum, client->buffer);
	
	//Update checkpoint
	checkpoint->end = end - image;

	//Write to disk
	rc = write(fd, image, imageSize);
	if(rc <= 0)
	  exit(-1);
	fsync(fd);

	//Setup the return message
	server->returnCode = returnCode;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
      }
      else{
	//Update checkpoint
	checkpoint->end = end - image;

    	//This is the case for shutdown
	rc = write(fd, image, imageSize);
	if(rc <= 0)
	  exit(-1);
	fsync(fd);

	//Set up the return message
	server->returnCode = 0;
	rc = UDP_Write(sd, &s, (char *)server, sizeof(ServerMessage_t));
	exit(0);
      }
    }
  }
  return 0;
}

int MyLookup(int pinum, char *name){
  //Variables
  int inum = -1;
  int i;
  MFS_Inode_t *inode;
  MFS_Dir_t *dir;
  MFS_DirEnt_t *dirEnt;
  int done = 0;

  //Check for invalid inum
  if(inodes[pinum] == NULL)
    return -1;

  printf("asldkf: %s\n", name);
  printf("length1: %d\n", strlen(name));

  //Get the inode and find info
  inode = inodes[pinum];
  dir = image + inode->addrs[0];
  for(i = 0; i < 128 && !done; i++){
    if(dir->entries[i] == -1)
      done = 1;
    dirEnt = image + dir->entries[i];
    printf("dirEnt Name: %s\n", dirEnt->name);
    printf("length2: %d\n", strlen(dirEnt->name));
    printf("dirEnt Num: %d\n", dirEnt->inum);
    if(strcmp(dirEnt->name, name)==0)
      inum = dirEnt->inum;
  }
  printf("Lookup inum: %d\n", inum);
  return inum;
}

int MyStat(int inum){
  //Variables
  MFS_Inode_t *inode;
  void *tmp;
  int count = 0;
  int i;

  //Check for invalid inum
  if(inodes[inum] == NULL)
    return -1;
  
  //Get the inode
  inode = inodes[inum];
  
  //Update server message
  server->type = inode->type;
  server->size = inode->size;
  
  //Count the number of blocks used
  for(i = 0; i < 14; i++){
    if(inode->addrs[i] != -1)
      count++;
  }
  server->blocksUsed = count;

  //Have to copy only certain length of blocks because a pointer
  //would end up copying everything from that point until the end
  for(i = 0; i < 14; i++){
    if(inode->addrs[i] != -1){
      tmp = image + inode->addrs[i];  
      memmove(server->blocks[i], tmp, BUFFER_SIZE);
    }
    else
      server->blocks[i] = NULL;
  }

  return 0;
}

int MyWrite(int inum, char *buffer, int block){
  printf("ENTERED WRITE 2\n");
  printf("inode: %d\n", inum);
  //Variables
  MFS_Inode_t *inode;
  void *location;

  //Check for invalid inum
  if(inodes[inum] == NULL)
    return -1;

  //Get the inode
  inode = inodes[inum];

  //Can't write into a directory
  if(inode->type == MFS_DIRECTORY)
    return -1;
  
  //Check for invalid block
  if(inode->addrs[block] != -1 || block < 0 || block > 13)
    return -1;

  //Put the information in the block
  location = position;
  memmove(location, buffer, BUFFER_SIZE);
  position += BUFFER_SIZE;

  //Update inode
  inode->addrs[block] = location - image;

  return 0;
}

int MyRead(int inum, int block){
  //Variables
  MFS_Inode_t *inode;
  void *tmp;

  //Check for invalid inum
  if(inodes[inum] == NULL)
    return -1;

  //Get the inode
  inode = inodes[inum];
  
  //Get the correct info
  if(inode->type == MFS_DIRECTORY){
    MFS_Dir_t *dir = image + inode->addrs[0];
    server->type = MFS_DIRECTORY;
    server->blocksUsed = dir->inum;
    strcpy(server->buffer, dir->name);
  }
  else{
    //Check for invalid block
    if(inode->addrs[block] == -1)
      return -1;
    
    //Set the info
    server->type = MFS_REGULAR_FILE;
    server->blocksUsed = inum;
    tmp = image + inode->addrs[block];
    memmove(server->buffer, tmp, BUFFER_SIZE);
  }

  return 0;
} 

int MyCreat(int pinum, int type, char *name){
  //Variables
  int len, rc;

  //Check for invalid inum
  if(inodes[pinum] == NULL)
    return -1;
  
  //Check to see if the name is too long
  len = strlen(name);
  if(len > 28)
    return -1;

  //Check to see if name already exists
  //INSERT CHECK HERE

  //Create the directory or file
  if(type == MFS_DIRECTORY)
    rc = addDirectory(pinum, name);
  else
    rc = createFile(pinum, name);

  return rc;
}

int MyUnlink(int pinum, char *name){
  //Variables
  MFS_Inode_t *childInode, *parentInode;
  MFS_Dir_t *dir;
  MFS_DirEnt_t *dirEnt;
  int cinum, i, spot;

  //Check for invalid pinum
  if(inodes[pinum] == NULL)
    return -1;

  //Lookup the specified inode
  cinum = MyLookup(pinum, name);
  if(cinum == -1)
    return 0;

  //Set the inodes
  parentInode = inodes[pinum];
  childInode = inodes[cinum];
 
  //2 Cases:  Directory or File
  if(childInode->type == MFS_DIRECTORY){
    dir = image + childInode->addrs[0];
    //Make sure that the directory is empty 
    for(i = 2; i < 128; i++){
      //Start at position 2 because there will always be at
      //least two entries for "." and ".."
      if(dir->entries[i] != -1)
	return -1;
    }
    //Directory is empty, have the parent unlink
    dir = image + parentInode->addrs[0];
    for(i = 0; i < 128; i++){
      dirEnt = dir->entries[i] + image;
      if(strcmp(name, dirEnt->name) == 1)
	spot = i;
      break;
    }
    dir->entries[i] = -1;
    inodes[cinum] = NULL;
  }else{
    //'name' is a file
    //Directory is empty, have the parent unlink
    dir = image + parentInode->addrs[0];
    for(i = 0; i < 128; i++){
      dirEnt = dir->entries[i] + image;
      if(strcmp(name, dirEnt->name) == 1)
	spot = i;
      break;
    }
    dir->entries[spot] = -1;
    inodes[cinum] = NULL;
  }
  return 0;
}

int createFile(int pinum, char *name){
  //Variables
  MFS_InodeMap_t *tempMap, *tempMap2;
  MFS_Inode_t *tempInode;
  MFS_Dir_t *tempDir;
  int inodeNumber, mapNumber;
  int mapPos = 0;
  int inodePos = 0;
  int pos = 0;
  int i;

  //Create the inode
  temp = position;
  inodeNumber = findFreeInode();
  inodePos = temp - image;
  tempInode = (MFS_Inode_t *)temp;
  tempInode->size = 0;
  tempInode->type = MFS_REGULAR_FILE;
  for(i = 0; i < 14; i++)
    tempInode->addrs[i] = -1;
  inodes[inodeNumber] = tempInode;
  position += sizeof(MFS_Inode_t);

  //Create the map piece
  mapNumber = findMapPiece(inodeNumber);
  if(inodeMap[mapNumber] == NULL){
    temp = position;
    mapPos = temp - image;
    tempMap = (MFS_InodeMap_t *)temp;
    tempMap->inodes[(inodeNumber%16)] = inodePos;
    inodeMap[mapNumber] = tempMap;
    position += sizeof(MFS_InodeMap_t);
  }
  else{
    temp = position;
    tempMap2 = inodeMap[mapNumber];
    mapPos = temp - image;
    tempMap = (MFS_InodeMap_t *)temp;
    for(i = 0; i < 16; i++)
      tempMap->inodes[i] = tempMap2->inodes[i];
    tempMap->inodes[(inodeNumber%16)] = inodePos;
    inodeMap[mapNumber] = tempMap;
    checkpoint->mapPieces[mapNumber] = mapPos;
    position += sizeof(MFS_InodeMap_t);
  }

  //Update the parent directory
  tempInode = inodes[pinum];
  tempDir = image + tempInode->addrs[0];
  for(i = 0; i < 128; i++){
    if(tempDir->entries[i] == -1){
      pos = i;
      break;
    }
  }
  printf("INODESSS: %d\n", inodeNumber);
  tempDir->entries[pos] = addDirectoryEntry(inodeNumber, name);
  tempInode->size += 4096;
  
  
  //Update end
  end = position;

  return 0;
}

int addDirectory(int pinum, char *name){
  //Variables
  MFS_InodeMap_t *tempMap, *tempMap2;
  MFS_Inode_t *tempInode;
  MFS_Dir_t *tempDir;
  int inodeNumber, mapNumber;
  int mapPos = 0;
  int inodePos = 0;
  int dirPos = 0;
  int dirEntPos = 0;
  int i;

  //Create the inode
  temp = position;
  inodeNumber = findFreeInode();
  inodePos = temp - image;
  tempInode = (MFS_Inode_t *)temp;
  tempInode->size = sizeof(MFS_Dir_t) + (2 * sizeof(MFS_DirEnt_t));
  tempInode->type = MFS_DIRECTORY;
  inodes[inodeNumber] = tempInode;
  position += sizeof(MFS_Inode_t);

  //Create the map piece
  mapNumber = findMapPiece(inodeNumber);
  if(inodeMap[mapNumber] == NULL){
    temp = position;
    mapPos = temp - image;
    tempMap = (MFS_InodeMap_t *)temp;
    tempMap->inodes[(inodeNumber%16)] = inodePos;
    inodeMap[mapNumber] = tempMap;
    position += sizeof(MFS_InodeMap_t);
  }
  else{
    temp = position;
    tempMap2 = inodeMap[mapNumber];
    mapPos = temp - image;
    tempMap = (MFS_InodeMap_t *)temp;
    for(i = 0; i < 16; i++)
      tempMap->inodes[i] = tempMap2->inodes[i];
    tempMap->inodes[(inodeNumber%16)] = inodePos;
    inodeMap[mapNumber] = tempMap;
    checkpoint->mapPieces[mapNumber] = mapPos;
    position += sizeof(MFS_InodeMap_t);
  }

  //Create the directory
  temp = position;
  dirPos = temp - image;
  tempInode->addrs[0] = dirPos;
  tempDir = (MFS_Dir_t *)temp;
  strcpy(tempDir->name, name);
  tempDir->inum = inodeNumber;
  for(i = 0; i < 128; i++){
    tempDir->entries[i] = -1;
  }
  position += sizeof(MFS_Dir_t);
 
  //Create the directory entries
  tempDir->entries[0] = addDirectoryEntry(inodeNumber, ".");
  tempDir->entries[1] = addDirectoryEntry(pinum, "..");

  //Update the parent directory
  if(strcmp(name, "/") == 1){
    tempInode = inodes[pinum];
    tempDir = image + tempInode->addrs[0];
    for(i = 0; i < 128; i++){
      if(tempDir->entries[i] == -1){
	dirEntPos = i;
	break;
      }
    }
    tempDir->entries[dirEntPos] = addDirectoryEntry(inodeNumber, name);
  }
  
  //Update end
  end = position;

  return 0;
}

int addDirectoryEntry(int inum, char *name){
  //Variables
  int dirEntPos = 0;
  MFS_DirEnt_t *tempDirEnt;

  //Add entry
  temp = position;
  dirEntPos = temp - image;
  tempDirEnt = (MFS_DirEnt_t *)temp;
  tempDirEnt->inum = inum;
  printf("NAME1: %s\n", name);
  strcpy(tempDirEnt->name, name);
  printf("NAME2: %s\n", tempDirEnt->name);
  position += sizeof(MFS_DirEnt_t);

  return dirEntPos;
}

int findFreeInode(){
  //Variable
  int inum, i;

  for(i = 0; i < 4096; i++){
    if(inodes[i] == NULL){
      inum = i;
      break;
    }
  }
  return inum;
}

int findMapPiece(int inode){
  //Variables
  int place;
  
  //Find the correct map piece
  place = inode/16;

  return place;
}

  /*
    int sd = UDP_Open(10000);
    assert(sd > -1);

    printf("waiting in loop\n");

    while (1) {
	struct sockaddr_in s;
	char buffer[BUFFER_SIZE];
	int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE);
	if (rc > 0) {
	    printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
	    char reply[BUFFER_SIZE];
	    sprintf(reply, "reply");
	    rc = UDP_Write(sd, &s, reply, BUFFER_SIZE);
	}
    }
  
    return 0;
  */

