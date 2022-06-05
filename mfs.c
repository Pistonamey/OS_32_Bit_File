/*  
  Name - Amey Shinde
  ID - 1001844387

   'a' used instead of i , because according to the 
    professor, i and j can be used for complex numbers.

    all the commands implemented correctly.

    'cd ..' implemented correctly.

    code taken from professors github : mfs.c, compare.c, LBAToOffset, NextLB

*/

// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
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

#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>


#define MAX_NUM_ARGUMENTS 5

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

FILE *fp;
int16_t BPB_RsvdSecCnt;
int32_t FirstDataSector = 0;
int8_t BPB_SecPerClus;
int32_t RootDirSector = 0;
int32_t BPB_FATSz32;
int8_t BPB_NumFATs;
int32_t FirstSectorOfCluster = 0;
int16_t BPB_RootEntCnt;
int16_t BPB_BytsPerSec;

bool x=false;

struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];
char saved[11]; // to copy the filename that was deleted

int compare(char IMG_Name[],char input[])
{
    char expanded_name[12];
    memset( expanded_name, ' ', 12 );

    char *token = (char*)malloc(sizeof(char) * 12);
    strncpy( token, input, 12 );
    strtok( token, "." );

    strncpy( expanded_name, token, strlen( token ) );

    token = strtok( NULL, "." );

    if( token )
    {
      strncpy( (char*)(expanded_name+8), token, strlen(token ) );
    }

    expanded_name[11] = '\0';

    int a;
    for( a = 0; a < 11; a++ )
    {
      expanded_name[a] = toupper( expanded_name[a] );
    }

    if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
    {
        return 1;
    }

    return 0;
}

/*
*Function    : LBAToOffset
*Parameters  : The current sector number that points to a block of data
*Returns     : The value of the address for that block of data
*Description : Finds the starting address of a block given the sector number
*corresponding to that block
*/
int LBAToOffset(int32_t sector)
{
    return (( sector - 2 ) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) +
                                    (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

/*
*Name:     NextLB
*Purpose:  Given a logical block address, look up into the first FAT and return
*the logical block address of the block in the file. If there is no further
*blocks then return -1.
*/
int16_t NextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

int main()
{

  char temp[100];
  int a;
  int fileOpen=0;
  char filen[12];

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
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    

    // open - opens the file system image.
    if(strcmp("open", token[0]) == 0)
    {
      if(fp)
      {
        printf("ERROR: File system image is already open\n");
      }
      else
      {
        fp=fopen(token[1],"r");
        
        if(fp)
        {
          //calculate bytes per Sector
          //for the given file
          fseek(fp, 11, SEEK_SET);
          fread(&BPB_BytsPerSec,2,1,fp);

          //calculate sectors per clusters
          //for the given file
          fseek(fp, 13, SEEK_SET);
          fread(&BPB_SecPerClus,1,1,fp);

          //calculate reserved sector count
          //for the given file
          fseek(fp, 14, SEEK_SET);
          fread(&BPB_RsvdSecCnt,2,1,fp);

          //calculate Number of FATs
          //for the given file
          fseek(fp,16, SEEK_SET);
          fread(&BPB_NumFATs,1,1,fp);

          //calculate FATzs32 
          //for the given file
          fseek(fp,36, SEEK_SET);
          fread(&BPB_FATSz32,4,1,fp);

          //Calculate root cluster
          int root_cluster = (BPB_NumFATs * BPB_FATSz32*BPB_BytsPerSec)+(BPB_RsvdSecCnt* BPB_BytsPerSec);
          fseek(fp,root_cluster,SEEK_SET);
          fread(&dir[0],sizeof(struct DirectoryEntry),16,fp);
          x=false;
        }
        else
        {
          printf("ERROR: File system image is not found\n");
        }
      }

    }

    //close the image file.
    if(strcmp("close", token[0])==0)
    {
      if(x)
      {
      printf("ERROR: File system must be opened first.\n");
      }
      else
      {
        if(fp)
        {
          fclose(fp);
          memset(dir,0,sizeof(dir));
          fp=NULL;
          x=true;
        }
        else
        {
          printf("ERROR: File System image not open.\n");

        }
      }
    }


    //info - print info of the image file (hex and decimal values)
    if(strcmp("info", token[0]) == 0)
    {

        if(x)
        {
           printf("ERROR: File must be opened first.\n");
        }

        else
        {
        
        printf("\n");
        printf("                \tDEC\tHEX\n");
        printf("BPB_BytesPerSec :\t%d\t%x\n",BPB_BytsPerSec,BPB_BytsPerSec);
        printf("BPB_SecPerClus  :\t%d\t%x\n",BPB_SecPerClus,BPB_SecPerClus);
        printf("BPB_RsvdSecCnt  :\t%d\t%x\n",BPB_RsvdSecCnt,BPB_RsvdSecCnt);
        printf("BPB_NumFATs     :\t%d\t%x\n",BPB_NumFATs,BPB_NumFATs);
        printf("BPB_FATSz32     :\t%d\t%x\n",BPB_FATSz32,BPB_FATSz32);
        }

        
      }


    //stat - show the stats of the file passed as argument
    if(strcmp("stat", token[0])==0)
    {
      int found =0;
      if(x)
      {
        printf("ERROR: File must be opened first.\n");
      }
      else
      {
         int index = -1;
        if(token[1]!=NULL)
         {
            for(a=0; a<16;a++)
               {
                
                 //checks if file is read only, a sub directory or an archive only.
                 if ((dir[a].DIR_Attr == 0x01)||dir[a].DIR_Attr == 0x10||dir[a].DIR_Attr == 0x20)           
                   {
                      memset(&filen, 0, 12);
                      strncpy(filen, dir[a].DIR_Name, 11);
                              
                       //check if its the same file       
                       if (compare(dir[a].DIR_Name,token[1])==1)
                        {
                        printf("Directory Name:    \t%s\n",filen);
                        printf("File size:         \t%d\n",dir[a].DIR_FileSize);
                        printf("First Cluster low: \t%d\n",dir[a].DIR_FirstClusterLow);
                        printf("DIR_ATTR :         \t%d\n",dir[a].DIR_Attr);
                        printf("First Cluster High:\t%d\n",dir[a].DIR_FirstClusterHigh);
                          found = 1;
                        }
                       else if (strcmp(dir[a].DIR_Name,token[1]) ==0)
                       {
                        if (dir[a].DIR_FirstClusterLow == 0)
                        {
                          dir[a].DIR_FirstClusterLow = 2;
                        }
                        printf("Directory Name:    \t%s\n",filen);
                        printf("File size:         \t%d\n",dir[a].DIR_FileSize);
                        printf("First Cluster low: \t%d\n",dir[a].DIR_FirstClusterLow);
                        printf("DIR_ATTR -         \t%d\n",dir[a].DIR_Attr);
                        printf("First Cluster High:\t%d\n",dir[a].DIR_FirstClusterHigh);
                        
                        found = 1;
                       }
                   }
              }
         }
         if(found == 0)
         {
          printf("ERROR: File Not found\n");
         }
      }
    }

    //takes the passed file from image file and places a copy
    // in our local directory.
    if(strcmp(token[0],"get")==0)
          {
            if(x)
              {
                  printf("Error: A file must be opened first.\n");
              }
              else
              {
                  int index = -1;
                  for (a = 0; a < 16; a++)
                  {
                    //checks if file is read only, a sub directory or an archive only.
                      if ((dir[a].DIR_Attr == 0x01)||dir[a].DIR_Attr == 0x10||dir[a].DIR_Attr == 0x20)
                      {
                          if(compare(dir[a].DIR_Name,token[1])==1)
                          {
                              fseek(fp, LBAToOffset(dir[a].DIR_FirstClusterLow), SEEK_SET);
                              FILE *getF = fopen(token[1], "w");// open the file as "write"
                              char write[512]; //writing/passing the data to the file 
                                              // in our directory.
                              
                              if(dir[a].DIR_FileSize<512)
                              {
                                  fread(&write[0], dir[a].DIR_FileSize, 1, fp);
                                  fwrite(&write[0],dir[a].DIR_FileSize, 1, getF);
                              }
                              else
                              {
                                  fread(&write[0], 512, 1, fp);
                                  fwrite(&write[0], 512, 1, getF);
                                  int size = dir[a].DIR_FileSize;
                                  size = size - 512;
                                  
                                  while (size > 0)
                                  {
                                      int cluster = NextLB(dir[a].DIR_FirstClusterLow);
                                      fseek(fp, LBAToOffset(cluster), SEEK_SET);
                                      fread(&write[0], 512, 1, fp);
                                      fread(&write[0], 512, 1, getF);
                                      size =size-512;
                                  }
                              }
                              fclose( getF );
                              index = 1;
                              break;
                          }
                          else
                          {
                              index =-1;
                          }
                      }
                  }
                  if(index == -1)
                  {
                      printf("Error: file not found\n");
                  }
                }
            }

    //change the directory to the argument passed
    if(strcmp(token[0],"cd")==0)
          {
              if(x)
              {
                printf("ERROR: File must be opened first.\n");
              }
              else
              {
                  for(a = 0; a < 16; a++)
                  {
                    //change to previous directory.
                      if (strcmp(token[1],".")==0 ||strcmp(token[1],"..")==0)
                      {
                          if(strstr(dir[a].DIR_Name,token[1]) != NULL)
                          {
                              if (dir[a].DIR_FirstClusterLow == 0)
                              {
                                  dir[a].DIR_FirstClusterLow = 2;
                              }
                              fseek(fp, LBAToOffset(dir[a].DIR_FirstClusterLow), SEEK_SET);
                              fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
                          }
                      }
                      else//change to another directory
                      {
                          strcpy(temp, token[1]);
                          if (compare(dir[a].DIR_Name,temp))
                          {
                              fseek(fp, LBAToOffset(dir[a].DIR_FirstClusterLow), SEEK_SET);
                              fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
                          }
                      }
                  }
              }
              
          }
          
    //list the files in the directory.
    if (strcmp(token[0], "ls")==0)
          {
              if(x)
              {
                printf("ERROR: File must be opened first.\n");
              }
              else
              {
                  for(a=0; a<16;a++)
                  {
                     //checks if file is read only, a sub directory or an archive only and 
                       if ((dir[a].DIR_Attr == 0x01 ||dir[a].DIR_Attr == 0x10 ||
                            dir[a].DIR_Attr == 0x20 )&& dir[a].DIR_Name[0]!= 0xffffffe5)
                       {
                           memset(&filen, 0, 12);
                           strncpy(filen, dir[a].DIR_Name,11);
                           printf("%s\n",filen);
                       }
                  }
              }
          }

    //read the file content from token[2] through token[3]
    if(strcmp(token[0],"read")==0)
     {
        if(x)
          {
              printf("Error: File must be opened first.\n");
          }
          else
          {
            for (a = 0; a < 16; a++)
              {
                strcpy(filen,token[1]);
                int noBytes = atoi(token[3]);// the number of bytes to read ahead
                int position = atoi(token[2]); // the start position      
                if (compare(dir[a].DIR_Name,filen)==1)
                  {
                    //checks if file is read only, a sub directory or an archive.
                    if ((dir[a].DIR_Attr == 0x01 || dir[a].DIR_Attr == 0x10 ||
                               dir[a].DIR_Attr == 0x20)) 
                      {
                      uint8_t bts; // stores the byte values.
                      fseek(fp, LBAToOffset(dir[a].DIR_FirstClusterLow), SEEK_SET);
                      fseek(fp, position, SEEK_CUR);
                      for(a = 0; a < noBytes; a++){
                      fread(&bts, 1, 1, fp);
                      printf("%d ", bts);
                      }
                      printf("\n");
                      }
                  }
              }
          }
    }

    //delete the file from the directory/image file
    if(strcmp("del",token[0])==0)
    {
      if(token[1]==NULL)// if nothing is passed as file name, give error
      {
        printf("Specify file name \n");
      }

      int file_index=0;
      if(file_index==-1)//if file not found
      {
        printf("File not found\n");
      }
      else
      {
        for(a=0; a<16;a++)
               {
                 //checks if file is read only, a sub directory or an archive.
                 if ((dir[a].DIR_Attr == 0x01)||dir[a].DIR_Attr == 0x10||dir[a].DIR_Attr == 0x20)           
                   {
                      memset(&filen, 0, 12);
                      strncpy(filen, dir[a].DIR_Name, 11);
                      if (compare(dir[a].DIR_Name,token[1])==1)
                        {
                          //save the file name in a duplicate variable.
                          strncpy(saved,dir[a].DIR_Name,11);
                          //dir[i].DIR_Name[0]=0Xe5;
                          dir[a].DIR_Name[0]='@'; //setting the first character of file
                                                  //to '@' (delete)
                          dir[a].DIR_Attr=0; //delete
                        }
                   }
               }
      }
    }

    //undel the file that was previously deleted.
    if(strcmp("undel",token[0])==0)
    {
      if(token[1]==NULL)//empty argument
      {
        printf("No file passed\n");
      }
      else
      {
        int pos =0;
        for(pos=0;pos<16;pos++)
        {
          if(dir[pos].DIR_Name[0]=='@')
          {
            //retrieving the file using the saved file name.
            strncpy(dir[pos].DIR_Name,saved,strlen(saved));
            dir[pos].DIR_Attr=0x1;
          }
        }
      }
    }
    


  //quit from mfs
    if(strcmp("quit", token[0])==0 || strcmp("exit", token[0])==0)
    {
      return 0;
    }



    free( working_root );

  }
  return 0;
}


