#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 1024

int main(void)
{
  int n, fd[2];
  pid_t pid;
  char parentline[MAXLINE];
  char childline[MAXLINE];
  if(pipe(fd) < 0)
  {
    fprintf(stderr, "pipe error\n");
  }
  else
  {
    int childno = 0;
    while(childno++ < 4)
    {
      if((pid = fork()) < 0)
      {
        fprintf(stderr, "fork error\n");
      }
      else if(pid == 0) /* we are in the child */
      {
        close(fd[0]);
        sprintf(childline, "I am child number %d\n", childno);
        write(fd[1], childline, strlen(childline));
      }
      else /* parent */
      {
        close(fd[1]);
        n = read(fd[0], parentline, MAXLINE);
        if(n > 0)
        {
          printf("The proud parent just received %d bytes from a child - here is the message\n", n);
          write(STDOUT_FILENO, parentline, n);
        }
      }
    }
  }
  return 0;
}

