#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)
#define MFS_BLOCK_SIZE   (4096)

//Syscall Number Codes
#define LOOKUP           (0)
#define STAT             (1)
#define WRITE            (2)
#define READ             (3)
#define CREAT            (4)
#define UNLINK           (5)
#define SHUTDOWN         (6)

typedef struct __MFS_Stat_t {
  int type;   // MFS_DIRECTORY or MFS_REGULAR
  int size;   // bytes
  int blocksUsed;
  void *blocks[14];
  // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_InodeMap_t{
  int inodes[16];
} MFS_InodeMap_t;

typedef struct __MFS_Checkpoint_t{
  int end;
  int mapPieces[256];
} MFS_Checkpoint_t;

typedef struct __MFS_Inode_t{
  int size;
  int type;
  int addrs[14];
} MFS_Inode_t;

typedef struct __MFS_DirEnt_t {
  char name[28];  // up to 28 bytes of name in directory (including \0)
  int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

typedef struct __MFS_Dir_t{
  char name[28];
  int inum;
  int entries[128];
} MFS_Dir_t;

typedef struct __ClientMessage_t{
  int syscallNumber;
  int type;
  int inum;
  int block;
  char buffer[MFS_BLOCK_SIZE];
} ClientMessage_t;

typedef struct __ServerMessage_t{
  int type;
  int size;
  int blocksUsed;
  void *blocks[14];
  char buffer[MFS_BLOCK_SIZE];
  int returnCode;
} ServerMessage_t;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);

#endif // __MFS_h__

