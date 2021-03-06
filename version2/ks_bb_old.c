#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#define MAXLINESIZE 1024
int buffSize;
FILE* fin;
struct item
{
  char* filename;
  int line_number;
  char* linestring;
};
struct global
{
int buffFront;
int buffEnd;
struct item* items;
pthread_mutex_t buffer_lock,bufferFront_lock,bufferEnd_lock;
volatile int running_threads;
pthread_mutex_t running_mutex;
char* pathToDir;
char* keyword;
int line_no;
};
struct for_thread
{
  char* filename;
  struct global this;
};
void newBuffer(struct global this)
{
  this.buffFront=0;
  this.buffEnd=-1;
  this.items=(struct item*)malloc(sizeof(struct item)*buffSize);
  return;
}
int isFull(struct global this)
{
  int ret=0;
  pthread_mutex_lock(&this.bufferFront_lock);
  pthread_mutex_lock(&this.bufferEnd_lock);
  if(this.buffFront==(this.buffEnd+2)%buffSize) ret=1;
  pthread_mutex_unlock(&this.bufferEnd_lock);
  pthread_mutex_unlock(&this.bufferFront_lock);
  return ret;
}
int isEmpty(struct global this)
{
  int ret=0;
  pthread_mutex_lock(&this.bufferFront_lock);
  pthread_mutex_lock(&this.bufferEnd_lock);
  if(this.buffFront==(this.buffEnd+1)%buffSize) ret=1;
  pthread_mutex_unlock(&this.bufferEnd_lock);
  pthread_mutex_unlock(&this.bufferFront_lock);
  return ret;
}
void insert_into_buffer(char* filename,int line_number,char* linestring,struct global this)
{
    while(isFull(this));//Wait for buffer to empty
    struct item newitem;
    newitem.filename=filename;
    newitem.line_number=line_number;
    newitem.linestring=linestring;
    pthread_mutex_lock(&this.bufferEnd_lock);
    this.items[this.buffEnd+1]=newitem;
    this.buffEnd++;
    printf("There are now %d items\n",this.buffFront-this.buffEnd);
    pthread_mutex_unlock(&this.bufferEnd_lock);
}
void destroyBuffer(struct global this)
{
  free(this.items);
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
  fflush(stdout);
  printf("Printer entered\n");
  struct global this=*(struct global*)a;
  printf("There are %d items",(this.buffFront-this.buffEnd));
  loop:
  while(this.running_threads!=0 || !isEmpty(this))
  {
    while(isEmpty(this)){if(this.running_threads==0) goto label;}
    fflush(stdout);
    pthread_mutex_lock(&this.bufferFront_lock);
    printf("%s:%d:%s\n",this.items[this.buffFront].filename,this.items[this.buffFront].line_number,this.items[this.buffFront].linestring);
    fflush(stdout);
    free(this.items[this.buffFront].filename);
    free(this.items[this.buffFront].linestring);
    this.buffFront++;
    pthread_mutex_unlock(&this.bufferFront_lock);
  }
  label:
  if(this.running_threads!=0 || !isEmpty(this)) goto loop;
  printf("Printer exit");
  pthread_exit(NULL);
}
void* worker(void* a)
{
  struct for_thread* args=(struct for_thread*)a;
  struct global this=args->this;
  char* filename=args->filename;
  free(args);
  char* fullFileName=(char*)malloc(sizeof(char)*MAXLINESIZE);
  strcpy(fullFileName,this.pathToDir);
  strcat(fullFileName,filename);
  FILE* file=fopen(fullFileName,"r");
  if(!file)
  {
    printf("Could not open file %s, line number: %d",fullFileName,this.line_no);
    pthread_exit(NULL);
  }
  printf("Opened: %s\n",fullFileName);
  free(fullFileName);
  char c;
  int line_number=1;
  char* fileLine=(char*)malloc(sizeof(char)*MAXLINESIZE);
  while(c!=EOF && !feof(file))
  {
    int i=0;
    while(!feof(file) && (c=fgetc(file)) && c!=EOF && c!='\n' )
    {
      fileLine[i]=c;
      i++;
    }
    fileLine[i]='\0';
    if(strcontains(fileLine,this.keyword,strlen(fileLine),strlen(this.keyword))==1)//insert item
    {
      char* printLine=(char*)malloc(sizeof(char)*MAXLINESIZE); 
      strcpy(printLine,fileLine);
      char* printFileName=(char*)malloc(sizeof(char)*MAXLINESIZE);
      strcpy(printFileName,filename);
      pthread_mutex_lock(&this.buffer_lock);
      printf("Inserting %s:%d:%s\n",printFileName,line_number,printLine);
      insert_into_buffer(printFileName,line_number,printLine,this);     
      pthread_mutex_unlock(&this.buffer_lock);
    }
    line_number++;
  }
  free(fileLine);
  fclose(file);
  free(filename);
  pthread_mutex_lock(&this.running_mutex);
  this.running_threads--;
  pthread_mutex_unlock(&this.running_mutex);
  pthread_exit(NULL);
}
int mainChild(int argc,char* argv[])
{
  fflush(stdout);
  printf("Child entered");
  struct global* this=malloc(sizeof(struct global));
  this.running_threads=0;
  this.pathToDir=argv[1];
  this.keyword=argv[2];
  this.line_no=atoi(argv[3]);
  buffSize=atoi(argv[4]);
  int i;
  newBuffer(this);
  int path_len=strlen(this.pathToDir);
  if(this.pathToDir[path_len-1]!='/')
  {
    char* newpath=malloc(sizeof(char)*(path_len+1));
    strcpy(newpath,this.pathToDir);
    strcat(newpath,"/");
    this.pathToDir=newpath;
  }
  if (pthread_mutex_init(&this.buffer_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",this.line_no);
      return;
  }
  if (pthread_mutex_init(&this.bufferFront_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",this.line_no);
      return;
  }
  if (pthread_mutex_init(&this.bufferEnd_lock, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",this.line_no);
      return;
  }
  if (pthread_mutex_init(&this.running_mutex, NULL) != 0)
  {
      printf("Lock init failed, line number: %d",this.line_no);
      return;
  }
  struct dirent *de=NULL;
  DIR *d=NULL;
  d=opendir(this.pathToDir);
  if(d == NULL)
  {
    printf("Couldn't open directory, line number: %d",this.line_no);
    return;
  }
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
      pthread_mutex_lock(&this.running_mutex);
      this.running_threads++;
      pthread_mutex_unlock(&this.running_mutex);
      struct for_thread* args=(struct for_thread*)malloc(sizeof(struct for_thread));
      args->filename=filename;
      args->this=this;
      pthread_create(&worker_tid,&attr,worker,args);
    }
  }
  pthread_create(&tid,&attr,printer,&this);
  while (this.running_threads > 0)
  {

  }
  pthread_join(tid,NULL);
  destroyBuffer(this);  
  closedir(d);
  pthread_mutex_destroy(&this.buffer_lock);
  pthread_mutex_destroy(&this.bufferFront_lock);
  pthread_mutex_destroy(&this.bufferEnd_lock);
  pthread_mutex_destroy(&this.running_mutex);
}
void hello()
{
  printf("Hi and hello there");
}
int main(int argc, char *argv[])
{
  int i,j;
  if(argc != 3 && argc!=5)
  {
    printf("Usage: %s pathToCommandFile bufferSize\n",argv[0]);
    return(1);
  }
  if(argc==5)
  {
    printf("Child reached");
    mainChild(argc,argv);
    return 1;
  }
  FILE* commandFile=fopen(argv[1],"r");
  fin=fopen("output.txt","a");
  buffSize=atoi(argv[2]);

  if(!commandFile)
  {
    printf("Could not open file %s",argv[1]);
    return(2);
  }
  char c='a';
  char* commandFileLine=(char*)malloc(sizeof(char)*(MAXLINESIZE+1));
  int line_no=0; 
  char* pathToDir;
  char* keyword; 
  pid_t pid;
  // if(pipe(fd) < 0)
  // {
  //   fprintf(stderr, "pipe error\n");
  // }
  // else
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
    commandFileLine[i]='\0';
    length=i;
    if(lastspace>=length || lastspace==0)
    {
      printf("Invalid command line, line number: %d",line_no);
      return(3);
    }
    else
    {
      pathToDir=(char*)malloc(sizeof(char)*(lastspace+1));
      keyword=(char*)malloc(sizeof(char)*(length-lastspace));
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
        // close(fd[0]);
        // sprintf(childline, "I am child number %d\n", line_no);
        // write(fd[1], childline, strlen(childline));
        // //child process
        // char* cmd=(char*)malloc(sizeof(char)*(12+length+20)); 
        //  char cwd[1024];
        //  getcwd(cwd,sizeof(cwd));
        //  strcat(cwd,"/child");
        char* argv[5];
        argv[0]="child";
        argv[1]=pathToDir;
        argv[2]=keyword;
        char line[10];
        sprintf(line,"%d",line_no);
        // strcpy(line,itoa(line_no));
        char buff_child[10];
        sprintf(buff_child,"%d",buffSize);
        // strcpy(buff_child,itoa(buffSize));
        argv[3]=line;
        argv[4]=buff_child;

        // printf("%s\n",cwd);
        // int temp_len=strlen(pathToDir);
        // sprintf(cmd,"child %s/ %s %d %d &",pathToDir,keyword,line_no,buffSize);
         // system(cmd);
        //execlp(argv[0],argv[0],argv[1],argv[2],argv[3],argv[4],NULL);
        // printf("%s %d\n",keyword,strlen(keyword));
        //system(cmd);
        // free(cmd);
        main(5,argv);
        //  free(pathToDir);
        // free(keyword);
        // // printf("This child");
        // mainChild(4,argv);
         // free(cmd);
        // //childProc(pathToDir,keyword,line_no);
        // (void)execl("child.c",argv[0],argv[1],argv[2],argv[3],NULL);
        // execlp(argv[0],argv[0],argv[1],argv[2],argv[3],argv[4],NULL);
        // perror("execlp");
        // mainChild(pathToDir,keyword,line_no);
        
        // hello();

                             
        exit(0);
      }
      else
      {

        // close(fd[1]);
               //parent process
        // join(pid);
        // free(pathToDir);
        // free(keyword);
        line_no++;
      }
    }
  }
  printf("Now waiting\n");
  for(i=0;i<line_no;i++)
    {
      printf("wait: %d\n",i);
        wait();
      }
  fflush(stdout);
  sleep(1);
  printf("done waiting");
  free(commandFileLine);
  fclose(commandFile);
  fclose(fin);
  return 1;
}