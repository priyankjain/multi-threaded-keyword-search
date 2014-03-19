#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#define MAXLINESIZE 1024
struct item
{
  char* filename;
  int line_number;
  char* linestring;
};
int buffFront;
int buffEnd;
struct item* items;
int buffSize;
int thread_count=0;
pthread_mutex_t buffer_lock,bufferFront_lock,bufferEnd_lock;
volatile int running_threads = 0;
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
char* pathToDir;
char* keyword;
int line_no;
void newBuffer()
{
  buffFront=0;
  buffEnd=-1;
  items=(struct item*)malloc(sizeof(struct item)*buffSize);
  return;
}
int isFull()
{
  int ret=0;
  pthread_mutex_lock(&bufferFront_lock);
  pthread_mutex_lock(&bufferEnd_lock);
  if(buffFront==(buffEnd+2)%buffSize) ret=1;
  pthread_mutex_unlock(&bufferEnd_lock);
  pthread_mutex_unlock(&bufferFront_lock);
  return ret;
}
int isEmpty()
{
  int ret=0;
  pthread_mutex_lock(&bufferFront_lock);
  pthread_mutex_lock(&bufferEnd_lock);
  if(buffFront==(buffEnd+1)%buffSize) ret=1;
  pthread_mutex_unlock(&bufferEnd_lock);
  pthread_mutex_unlock(&bufferFront_lock);
  return ret;
}
void insert_into_buffer(char* filename,int line_number,char* linestring)
{
    while(isFull());//Wait for buffer to empty
    struct item newitem;
    newitem.filename=filename;
    newitem.line_number=line_number;
    newitem.linestring=linestring;
    pthread_mutex_lock(&bufferEnd_lock);
    items[buffEnd+1]=newitem;
    buffEnd++;
    pthread_mutex_unlock(&bufferEnd_lock);
}
void remove_from_buffer()
{
    while(isEmpty());
    free(items[buffFront].filename);
    free(items[buffFront].linestring);
    buffFront++;
}
void destroyBuffer()
{
  free(items);
}
int strcontains(char* fileline,char* keyword,int fileLength,int keywordLength)
{
  int i,j;
  for(i=0;i<fileLength-keywordLength+1;i++)
  {
    if(i==0 || (!(fileline[i-1]>='a' && fileline[i-1]<='z') && !(fileline[i-1]>='A' && fileline[i-1]<='Z')))
    {
      for(j=0;j<keywordLength;j++)
      {
        if(tolower(keyword[j])==tolower(fileline[i+j]));
        else break;
      }
      if(j==keywordLength && (fileline[i+j]=='\0' || fileline[i+j]=='\n' || (!(fileline[i+j]>='a' && fileline[i+j]<='z') && !(fileline[i+j]>='A' && fileline[i+j]<='Z')))) {return 1;}
    }
  }
  return 0;
}

void* printer(void* a)
{
  loop:
  while(running_threads!=0 || !isEmpty())
  {
    while(isEmpty()){if(running_threads==0) goto label;};
    pthread_mutex_lock(&bufferFront_lock);
    printf("%s:%d:%s\n",items[buffFront].filename,items[buffFront].line_number,items[buffFront].linestring);
    free(items[buffFront].filename);
    free(items[buffFront].linestring);
    buffFront++;
    pthread_mutex_unlock(&bufferFront_lock);
  }
  label:
  if(running_threads!=0 || !isEmpty()) goto loop;
  pthread_exit(NULL);
}
void* worker(void* filename)
{
  char* fullFileName=(char*)malloc(sizeof(char)*MAXLINESIZE);
  strcpy(fullFileName,pathToDir);
  strcat(fullFileName,filename);
  FILE* file=fopen(fullFileName,"r");
  if(!file)
  {
    printf("Could not open file %s, line number: %d",fullFileName,line_no);
    pthread_exit(NULL);
  }
  char c;
  int line_number=1;
  char* fileLine=(char*)malloc(sizeof(char)*MAXLINESIZE);
  while(c!=EOF)
  {
    int i=0;
    while(!feof(file) && (c=fgetc(file)) && c!=EOF && c!='\n' )
    {
      fileLine[i]=c;
      i++;
    }
    fileLine[i]='\0';
    if(strcontains(fileLine,keyword,strlen(fileLine),strlen(keyword))==1)//insert item
    {
      char* printLine=(char*)malloc(sizeof(char)*MAXLINESIZE); 
      strcpy(printLine,fileLine);
      char* printFileName=(char*)malloc(sizeof(char)*MAXLINESIZE);
      strcpy(printFileName,filename);
      pthread_mutex_lock(&buffer_lock);
      insert_into_buffer(printFileName,line_number,printLine);     
      pthread_mutex_unlock(&buffer_lock);
    }
    line_number++;
  }
  free(fileLine);
  fclose(file);
  free(filename);
  pthread_mutex_lock(&running_mutex);
  running_threads--;
  pthread_mutex_unlock(&running_mutex);
  free(fullFileName);
  pthread_exit(NULL);
}
int main(int argc,char* argv[])
{ 
  pathToDir=argv[1];
  keyword=argv[2];
  line_no=atoi(argv[3]);
  buffSize=atoi(argv[4]);
  int i;
  newBuffer();
  if (pthread_mutex_init(&buffer_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",line_no);
      return;
  }
  if (pthread_mutex_init(&bufferFront_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",line_no);
      return;
  }
  if (pthread_mutex_init(&bufferEnd_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",line_no);
      return;
  }
  struct dirent *de=NULL;
  DIR *d=NULL;
  d=opendir(pathToDir);
  if(d == NULL)
  {
    printf("Couldn't open directory, line number: %d",line_no);
    return;
  }
  int thread_count=0;
  char a;
  pthread_t tid;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_t worker_tid;
  // Loop while not NULL
  while(de = readdir(d))
  {

    if(de->d_name[0]=='.')
      continue;
    else
    {
      char* filename=(char*)malloc(sizeof(char)*MAXLINESIZE);
      strcpy(filename,de->d_name);
      char c;
      pthread_mutex_lock(&running_mutex);
      running_threads++;
      pthread_mutex_unlock(&running_mutex);
      pthread_create(&tid,&attr,worker,filename);
    }
  }
  pthread_create(&tid,&attr,printer,NULL);
  while (running_threads > 0)
  {
  }
  pthread_join(tid,NULL);
  destroyBuffer();  
  closedir(d);
  pthread_mutex_destroy(&buffer_lock);
  pthread_mutex_destroy(&bufferFront_lock);
  pthread_mutex_destroy(&bufferEnd_lock);
}