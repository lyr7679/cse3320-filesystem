// The MIT License (MIT)
//
// Copyright (c) 2016, 2017 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//del still kind of weird
//have to set blocks to be able to be written over but then i dont know how
//  undel will work, how will that directory know when its data has been written over?
//still need get commands
//list command supa scuffed,seg faults i have no idea whats going on in there, may be a hard
//  reset angle tbh, shouldnt be that hard
//savefs, open should be good
//removed the second set of ifs in the attrib, think it will only be one or the other
//  wont be two commands at once, probably need to change the passed in filename to the
//  attrib command to be token[2]
//also do we want createfs to change data_blocks?
//  shouldnt data_blocks only be changed after we call open?
//  would need to have a reset data-blocks function
//also probably need to reset everything if we close because technically if we close
//  we can still use list command and other stuff and I dont think thats how it should work
//  or maybe just have a conditional for if the currentFp == NULL then only open, createfs,
//  and quit can be run
//also made a global FILE currentFp to be the file that we are currently using, will be NULL
//  if no file is currently open, line 93 is where its initialized

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#define NUM_INODES 125          //assigned number of inodes in system
#define NUM_BLOCKS 4226         //number of allocated blocks
#define BLOCK_SIZE 8192         //size of each allocated block
#define MAX_FILE_NUM 125        //max number of supported files
#define MAX_BLOCKS_PER_FILE 1250  //max size of each supported file
#define MAX_FILE_NAME 32        //max number of characters allowed for a file name

unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];
// int used_blocks[NUM_BLOCKS];

struct directory_entry{
  char name[MAX_FILE_NAME];
  int valid;
  int inode_idx;
};

struct directory_entry *directory_ptr;

//attributes have been moved to inode struct
struct inode{
  time_t date;
  int valid;
  int filesize;
  int hidden;
  int read_only;
  int blocks[MAX_BLOCKS_PER_FILE];
};

struct inode * inode_arr_ptr[NUM_INODES];
FILE *currentFp = NULL;

int findFreeDirectory();
int findDeletedDirectory();
int findFreeInode();
int findFreeBlock();
int findFreeInodeBlock(int inode_idx);
int valid_commands(char *token[]);
void createfs_command(char *);
void put_command(char *);
void list_command(char *token[]);
void attrib_command(char *token[]);
int df_command();
void initDirectory();
void initInodes();
void initFreeBlocks();
void initFreeInodes();
int delInode(char *filename);
int undelInode(char *filename);
void openfs(char *filename);
void savefs();

int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    //this will handle the quit command
    if(!strcmp(token[0],"quit"))
    {
      free(working_root);
      break;
    }

//    Now print the tokenized input as a debug check
//    \TODO Remove this code and replace with your shell functionality
    //
    // int token_index  = 0;
    // for( token_index = 0; token_index < token_count; token_index ++ )
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );
    // }

    //clearing b/c sometimes we can get leftover garbage which throws results
    //for later loops

    free( working_root );

    int valid_cmd = valid_commands(token);

    if(valid_cmd == 1)
    {
      printf("no\n");
    }

    //confirmation check after every loop
    // printf("Disk free: %d bytes\n", df_command());

    for(int i = 0; i < token_count; i++)
    {
        token[i] = NULL;
    }

  }
  return 0;
}


//if the valid parameter is equal to zero, that means the directory entry
//is free to use
//we will have at max 125 directories (one per file) so to find a free
//dir we can just go through our directory_ptr arrray which has already been
//initialized at the start to have valid as 0
int findFreeDirectory()
{
  int returnval = -1;

  for(int i = 0; i < MAX_FILE_NUM; i++)
  {
    if(directory_ptr[i].valid == 0)
    {
      returnval = i;
      break;
    }
  }
  return returnval;
}

int findDeletedDirectory()
{
  int returnval = -1;

  for(int i = 0; i < MAX_FILE_NUM; i++)
  {
    if(directory_ptr[i].valid == -1)
    {
      returnval = i;
      break;
    }
  }
  //we need to clear inode blocks  of this directory entry
  //we'll be reusing
  if(returnval != -1)
  {
    inode_arr_ptr[directory_ptr[returnval].inode_idx]->valid = 0;

    for(int i = 0; i < MAX_BLOCKS_PER_FILE; i++)
      inode_arr_ptr[directory_ptr[returnval].inode_idx]->blocks[i] = 0;
  }
  return returnval;
}

//he told us to use block 2 as a inode map so in the initilization im just
//filling all of data_block 2 with F (for false as in free) and true is filled
//however since the inode blocks are 5 - 130, im only looking for indexes through
//that point. aka we will never use 0-4 or 131 - 8192 bytes of this block
int findFreeInode()
{
  int returnval = -1;

  for(int i = 5; i < 130; i++)
  {
    if(data_blocks[2][i] == 'F')
    {
      returnval = i;
      break;
    }
  }
  return returnval;
}

//looking at block 3 b/c thats supposed to be reserved for the free block map
//same as inodes where we fill all during initialization but since free data blocks
//are only 131 - 4226, im only looking/returning at those indexes
int findFreeBlock()
{
  int returnval = -1;

  for(int i = 131; i < NUM_BLOCKS; i++)
  {
    if(data_blocks[3][i] == 'F')
    {
      returnval = i;
      break;
    }
  }
  return returnval;
}

//inode block is not in use if index is -1
//pretty sure this is just to make surre that when you're writing to the
//inode block array and sayiing what blocks are being used for that file,
//you're making sure to put them in sequential order
int findFreeInodeBlock(int inode_idx)
{
  int returnval = -1;
  for(int i = 0; i < MAX_BLOCKS_PER_FILE; i++)
  {
    if(inode_arr_ptr[inode_idx]->blocks[i] == -1);
    {
      returnval = i;
      break;
    }
  }
  return returnval;
}

//this is a function that branches off to all valid commands we need to
//account for. If it returns -1 to main, that meeans we have an invalid command
//and can print the appropriate message in main
//all functions called will have a return 0 to exit this function, but any errors
//and printing error messages will be handled in the respective functions, not here
int valid_commands(char *token[])
{
    FILE *fp;
    if(token[0] == NULL)
    {
        return -1;
    }
    //if strcmp matches with history, we call our history function
    //that takes care of the functionality of the "history" command
    else if(!strcmp(token[0], "put"))
    {
        if(token[1] == NULL)
        {
          printf("put: too few arguments\n");
        }
        put_command(token[1]);
    }
    //if strcmp matches with listpids, we call our print pids function
    //that takes care of the functionality of the "listpids" command
    else if(!strcmp(token[0], "df"))
    {
      if(df_command() == -1)
      {
        return 0;
      }
        printf("%d bytes free\n", df_command());
    }
    //if strcmp matches with cd, we call chdir
    //that takes care of the functionality of the "cd" command
    else if(!strcmp(token[0], "list"))
    {
        list_command(token);
    }
    else if(!strcmp(token[0], "attrib"))
    {
      attrib_command(token);
    }
    else if(!strcmp(token[0], "createfs"))
    {
      if(token[1] == NULL)
      {
        printf("createfs: File not found.\n");
      }
      createfs_command(token[1]);
    }
    else if(!strcmp(token[0], "open"))
    {
      openfs(token[1]);
    }
    else if(!strcmp(token[0], "del"))
    {
      delInode(token[1]);
    }
    else if(!strcmp(token[0], "undel"))
    {
      undelInode(token[1]);
    }
    else if(!strcmp(token[0], "savefs"))
    {
      savefs(fp);
    }
    else if(!strcmp(token[0], "close"))
    {
      fclose(currentFp);
      currentFp = NULL;
    }
    //if all conditions do not apply, return -1 to set flag
    //means command not found
    else
        return 1;
    return 0;
}


//super long function to handle put (ew)
//comments are all in function below
void put_command(char *filename)
{
  if(currentFp == NULL)
  {
    printf("put: Not currently in a filesystem\n");
    return;
  }
  int status = 0;
  //used for checking to see if we have a valid file to open
  struct stat buf;

  //since we only allow for 32 chars we have to check this
  //i still need to account for requirement 1.9 which doesn't totally make sense
  if(strlen(filename) > MAX_FILE_NAME)
  {
    printf("put error: File name too long.\n");
    return;
  }
  // Call stat with out input filename to verify that the file exists.  It will also
  // allow us to get the file size. We also get interesting file system info about the
  // file such as inode number, block size, and number of blocks.  For now, we don't
  // care about anything but the filesize.
  status =  stat( filename, &buf );

  // If stat did not return -1 then we know the input file exists and we can use it.
  if( status != -1 )
  {
    //buf.st_size is the file size in bytes. We divide this by block size to get the
    //amount of blocks the file is. If it's greater than the max allowed blocks per
    //file or there just isn't enough space, which is checked with df_command, then
    //we print out the appropriate error
    if((buf.st_size / BLOCK_SIZE) > MAX_BLOCKS_PER_FILE || buf.st_size > df_command())
    {
      printf("put error: Not enough disk space.\n");
      return;
    }

    //we need to find a free directory entry to insert this file
    int dir_idx = findFreeDirectory();
    //if we cant find an open entry we can now check for "deleted files"
    if(dir_idx == -1)
    {
      dir_idx = findDeletedDirectory();
    }
    if(dir_idx == -1)
    {
      printf("put error: Not enough disk space.\n");
      return;
    }

    //marks this index as valid so we don't accidentally overwrite it When
    //adding more files later
    directory_ptr[dir_idx].valid = 1;

    //allocate memory for filename
    // directory_ptr[dir_idx].name = (char *) malloc(strlen(filename));
    //storing file name into directory struct
    strncpy(directory_ptr[dir_idx].name, filename, strlen(filename));

    //finding a free inode to save off index of where this file is stored
    int inode_idx = findFreeInode();

    if(inode_idx == -1)
    {
      printf("put error: No free inodes.\n");
      return;
    }

    //store inode index of file in directory ptr
    directory_ptr[dir_idx].inode_idx = inode_idx;

    //file always starts off with both attributes "off"
    inode_arr_ptr[inode_idx]->hidden = 0;
    inode_arr_ptr[inode_idx]->read_only = 0;

    //now that we have the inode index we can store information like time, size,
    //the fact that it's in use, etc
    inode_arr_ptr[inode_idx]->filesize = buf.st_size;
    inode_arr_ptr[inode_idx]->date = time(NULL);
    inode_arr_ptr[inode_idx]->valid = 1;

    // Open the input file read-only
    FILE *ifp = fopen ( filename, "r" );
    printf("Reading %d bytes from %s\n", (int) buf.st_size, filename );

    // Save off the size of the input file since we'll use it in a couple of places and
    // also initialize our index variables to zero.
    int copy_size   = buf.st_size;

    // We want to copy and write in chunks of BLOCK_SIZE. So to do this
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int offset      = 0;

    // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big
    // memory pool. Why? We are simulating the way the file system stores file data in
    // blocks of space on the disk. block_index will keep us pointing to the area of
    // the area that we will read from or write to.
    int block_index = findFreeBlock();

    if(block_index == -1)
    {
      printf("something is wrong\n");
      return;
    }

    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size >= BLOCK_SIZE )
    {
      block_index = findFreeBlock();

      if(block_index == -1)
      {
        printf("something is wrong\n");
        return;
      }

      //letting filesystem know this specific block is now in use
      data_blocks[3][block_index] = 1;

      int inode_block = findFreeInodeBlock(inode_idx);

      if(inode_block == -1)
      {
        printf("something is wrong\n");
        return;
      }
      //tbh im not too sure about this one
      //slightly iffy on how blocks/inodes/inode blocks are all connected
      //pretty sure im just going into the initial inode index we deemed free
      //at the start, and then within that, going to the first free block of
      //the inode and storing the index of what block (131-4226) we have stored
      //this specific file data at
      inode_arr_ptr[inode_idx]->blocks[inode_block] = block_index;


      // Index into the input file by offset number of bytes.  Initially offset is set to
      // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We
      // then increase the offset by BLOCK_SIZE and continue the process.  This will
      // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
      fseek( ifp, offset, SEEK_SET );

      // Read BLOCK_SIZE number of bytes from the input file and store them in our
      // data array.
      int bytes  = fread( data_blocks[block_index], BLOCK_SIZE, 1, ifp );

      // If bytes == 0 and we haven't reached the end of the file then something is
      // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
      // It means we've reached the end of our input file.
      if( bytes == 0 && !feof( ifp ) )
      {
        printf("An error occured reading from the input file.\n");
        return;
      }

      // Clear the EOF file flag.
      clearerr( ifp );

      // Reduce copy_size by the BLOCK_SIZE bytes.
      copy_size -= BLOCK_SIZE;

      // Increase the offset into our input file by BLOCK_SIZE.  This will allow
      // the fseek at the top of the loop to position us to the correct spot.
      offset    += BLOCK_SIZE;

      // Increment the index into the block array
      block_index ++;
    }

    //special case for when file is not an exact multiple of block size and we
    //have some bytes left over needed to copy
    //same process the only thing that changes is in fread we just read the
    //remaining copy sizee, not the full block size like before
    if(copy_size > 0)
    {
      block_index = findFreeBlock();

      if(block_index == -1)
      {
        printf("something is wrong\n");
        return;
      }

      int inode_block = findFreeInodeBlock(inode_idx);
      if(inode_block == -1)
      {
        printf("something is wrong\n");
        return;
      }
      inode_arr_ptr[inode_idx]->blocks[inode_block] = block_index;

      data_blocks[3][block_index] = 1;

      fseek( ifp, offset, SEEK_SET );
      int bytes  = fread( data_blocks[block_index], copy_size, 1, ifp );
    }
    // We are done copying from the input file so close it out.
    fclose( ifp );
  }

  //this else goes back to whether the file was even valid or not, which was
  //determined by stat( )
  else
  {
    printf("Unable to open file: %s\n", filename );
    perror("Opening the input file returned: ");
    return;
  }

}

//function for list command
//not sure how i should be looping through but i think just checking to see if
//there's even something stored in the name works sincee its just a struct ptr
//if we list with the argument -h we want to print all valid files. If we have
//no argument, we want to print only the valid files that have a 0 for the
//hidden attribute
//the lines where im subtracting 6 and setting to a null pointer is because if you
//print the date as a string (not int) it includes the year and a newline char, which
//according to his requirements we dont want. so to get rid of the year and newline,
//im just going back six and cutting off the string there
void list_command(char *token[])
{
  if(currentFp == NULL)
  {
    printf("list: Not currently in a filesystem\n");
    return;
  }
  int inode_idx = 0;
  int dir_idx;
  char *str;
  int flag = 0;

  for(int dir_idx = 0; dir_idx < MAX_FILE_NUM; dir_idx++)
  {
    if(token[1] == NULL)
    {
      if(directory_ptr[dir_idx].valid == 1)
      {
      inode_idx = directory_ptr[dir_idx].inode_idx;
      str = ctime(&inode_arr_ptr[inode_idx]->date);
      str[strlen(str) - 9] = '\0';
      if(inode_arr_ptr[inode_idx]->hidden != 1)
      {
        flag++;
        printf("%d %s %s\n", inode_arr_ptr[inode_idx]->date, str, directory_ptr[dir_idx].name);
      }
      }
    }
    else if(!strcmp(token[1], "-h"))
    {
      if(directory_ptr[dir_idx].valid == 1)
      {
        flag++;
        inode_idx = directory_ptr[dir_idx].inode_idx;
        str = ctime(&inode_arr_ptr[inode_idx]->date);
        str[strlen(str) - 9] = '\0';
        printf("%d %s %s\n", inode_arr_ptr[inode_idx]->date, str, directory_ptr[dir_idx].name);
      }
    }
  }

  if(flag == 0)
  {
    printf("list:No files found\n");
  }
}

//i get that this is super scuffed but i expired my peak coding hours
//pretty much i just find the filename and then loop through the directory_ptr
//until i find a filename match. If there is one, set a flag that we found a
//match and strcmp all possible options to adjust the corresponding attributes in
//the struct (accounts for if you change both attributes at one time)
//if the flag is still set to zero by the end of it, we print out that the file
//wasn't found
//removed the second set of ifs, not needed, will be one or the other not both at the same time
void attrib_command(char *token[])
{
  if(currentFp == NULL)
  {
    printf("attrib: Not currently in a filesystem\n");
    return;
  }

  int dir_idx = 0;
  int found = 0;
  char * filename;

  if(token[1] == NULL || token[2] == NULL)
  {
      printf("attrib: Not enough arguments.\n");
      return;
  }

  if(token[1][0] == '-' || token[1][0] == '+')
  {
    if((token[2][0] == '-' || token[2][0] == '+'))
      filename = token[3];
    else
      filename = token[2];
  }

  for(int dir_idx = 0; dir_idx < MAX_FILE_NUM; dir_idx++)
  {
    if(!strcmp(directory_ptr[dir_idx].name, filename))
    {
      found = 1;
      int inode_idx = directory_ptr[dir_idx].inode_idx;

      if(!strcmp(token[1], "+h"))
        inode_arr_ptr[inode_idx]->hidden = 1;
      else if(!strcmp(token[1], "-h"))
        inode_arr_ptr[inode_idx]->hidden = 0;
      else if(!strcmp(token[1], "+r"))
        inode_arr_ptr[inode_idx]->read_only = 1;
      else if(!strcmp(token[1], "-r"))
        inode_arr_ptr[inode_idx]->read_only = 0;

      if(strcmp(filename, token[2]))
      {
        if(!strcmp(token[2], "+h"))
         inode_arr_ptr[inode_idx]->hidden = 1;
       else if(!strcmp(token[2], "-h"))
         inode_arr_ptr[inode_idx]->hidden = 0;
       else if(!strcmp(token[2], "+r"))
         inode_arr_ptr[inode_idx]->read_only = 1;
       else if(!strcmp(token[2], "-r"))
         inode_arr_ptr[inode_idx]->read_only = 0;
      }
      break;
    }
    dir_idx++;
  }
  if(found != 1)
    printf("attrib: File not found.\n");
}

//this just returns the number of free bytes
//i start from 131 bc he said use blocks 5-130 for inodes so our
//data blocks dont start until 131
//if the data_blocks[3] index , it means its free and we increment
//the count, which gives us the number of free blocks
//since it's supposed to be the freeee block map
//then we just multiply by the block size to get the number of bytes.
int df_command()
{
  if(currentFp == NULL)
  {
    printf("df: Not currently in a filesystem\n");
    return -1;
  }

  int count = 0;

  for(int i = 131; i < NUM_BLOCKS; i++)
  {
    if(data_blocks[3][i] == 'F')
      count++;
  }
  return (count * BLOCK_SIZE);
}


//creates a file system
//a file system is pretty much just a folder for all the other stuff
//so we're making this new file/folder, then initializing all of our
//blocks and struct *, tying the struct * with the actual data_block array,
//and then writing the entire data_block array to this filesystem


//so for open you'd just need to open the file pointer of the filesystem name
//you were searching for, and for close you'd just close it to make sure no one
//can write to it anymore
//and for save you would rewrite your block struct to the filepointer to save
//the new version
void createfs_command(char * image_name)
{
  FILE *fp;
  fp = fopen(image_name, "w+");
  memset(&data_blocks[0], 0 , NUM_BLOCKS * BLOCK_SIZE);
  initFreeBlocks();
  initFreeInodes();

  directory_ptr = (struct directory_entry *)&data_blocks[0];
  initDirectory();

  for(int i = 0; i < NUM_INODES; i++)
  {
    inode_arr_ptr[i] = (struct inode *)&data_blocks[i+5];
  }
  initInodes();

  fwrite(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, fp);
  fclose(fp);
}

//all of the below are just init functions, wherre we're setting the base values
//for everything
void initDirectory()
{
  for(int i = 0; i < MAX_FILE_NUM; i++)
  {
    memset(directory_ptr[i].name, 0 , 32 * sizeof(char));
    directory_ptr[i].valid = 0;
    directory_ptr[i].inode_idx = -1;
  }
}

void initInodes()
{
  for(int i = 0; i < NUM_INODES; i++)
  {
    inode_arr_ptr[i]->date = -1;
    inode_arr_ptr[i]->filesize = -1;
    inode_arr_ptr[i]->valid = 0;
    inode_arr_ptr[i]->hidden = 0;
    inode_arr_ptr[i]->read_only = 0;
    for(int j = 0; j < MAX_BLOCKS_PER_FILE; j++)
    {
      inode_arr_ptr[i]->blocks[j] = -1;
    }
  }
}

void initFreeBlocks()
{
  for(int i = 0; i < NUM_BLOCKS + 131; i++)
  {
    data_blocks[3][i] = 'F';
  }
}

void initFreeInodes()
{
  for(int i = 0; i < NUM_INODES + 5; i++)
  {
    data_blocks[2][i] = 'F';
  }
}

int delInode(char *filename)
{
  if(currentFp == NULL)
  {
    printf("del: Not currently in a filesystem\n");
    return -1;
  }

  for(int i = 0; i < NUM_INODES; i++)
  {
    if(!strcmp(directory_ptr[i].name, filename))
    {
      inode_arr_ptr[directory_ptr[i].inode_idx]->valid = -1;
      printf("Successfully deleted file %s\n", filename);
      return 0;
    }
  }
  printf("File %s does not exist\n", filename);
  return -1;
}

int undelInode(char *filename)
{
  if(currentFp == NULL)
  {
    printf("undel: Not currently in a filesystem\n");
    return -1;
  }

  for(int i = 0; i < NUM_INODES; i++)
  {
    if(!strcmp(directory_ptr[i].name, filename) && inode_arr_ptr[directory_ptr[i].inode_idx]->valid == -1)
    {
      inode_arr_ptr[directory_ptr[i].inode_idx]->valid = 0;
      printf("File %s successfully recovered \n", filename);
      return 0;
    }
  }
  printf("File %s could not be recovered\n", filename);
  return -1;
}

void openfs(char *filename)
{
  if(currentFp == NULL)
  {
    currentFp = fopen(filename, "r+");
    if(currentFp == NULL) {
      printf("invalid filename\n");
    }
  }
  else
  {
    printf("Another file is already open\nClose before opening %s\n", filename);
    return;
  }

  fread(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, currentFp);
  directory_ptr = (struct directory_entry *)&data_blocks[0];
  for(int i = 0; i < NUM_INODES; i++)
  {
    inode_arr_ptr[i] = (struct inode *)&data_blocks[i+5];
  }
}

void savefs()
{
  if(currentFp == NULL)
  {
    printf("savefs: Not currently in a filesystem\n");
    return;
  }

  rewind(currentFp);
  fwrite(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, currentFp);
}
