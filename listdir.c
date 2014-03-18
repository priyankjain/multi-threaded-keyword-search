#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define MAXLINESIZE 1024
int buffSize;
int main(int argc, char *argv[])
{
  int i,j;
  if(argc != 3)
  {
    printf("Usage: %s pathToCommandFile bufferSize\n",argv[0]);
    return(1);
  }
  FILE* commandFile=fopen(argv[1],"r");
  buffSize=atoi(argv[2]);
  if(!commandFile)
  {
    printf("Could not open file %s",argv[1]);
    return(2);
  }
  char c='a';
  char* commandFileLine=(char*)malloc(sizeof(char)*(MAXLINESIZE+1));
  int line_no=0;
  while(c!=EOF)
  {
    i=0;
    int length=0;
    int lastspace=0;
    while((c=fgetc(commandFile))!='\n' && c!=EOF)
    {
      if(c==' ')
        lastspace=i;
      commandFileLine[i]=c;
      i++;
    }

    length=i;
    if(lastspace>=length || lastspace==0)
    {
      printf("Invalid command line, line number: %d",line_no);
      return(3);
    }
    else
    {
      char* pathToDir=(char*)malloc(sizeof(char)*(lastspace+1));
      char* keyword=(char*)malloc(sizeof(char)*(length-lastspace));
      for(j=0;j<lastspace;j++)
      {
        pathToDir[j]=commandFileLine[j];
      }
      pathToDir[j]='\0';
      for(j=0;j<length-lastspace;j++)
      {
        keyword[j]=commandFileLine[lastspace+j+1];
      }
      keyword[j]='\0';
      pid_t pid;
      pid=fork();
      if(pid<0)
      {
        printf("Fork Failed");
        free(pathToDir);
        free(keyword);
        return 1;
      }
      else if(pid==0)
      {
        //child process
        char* cmd=(char*)malloc(sizeof(char)*(12+length+20));
        sprintf(cmd,"./hello.o %s %s %d",pathToDir,keyword,line_no,buffSize);
        system(cmd);
        free(cmd);
        //childProc(pathToDir,keyword,line_no);
        exit(0);
      }
      else
      {
        //parent process
        line_no++;
      }
    }
  }
  for(i=0;i<line_no;i++)
    wait();
  free(commandFileLine);
  fclose(commandFile);
  return(0);
}