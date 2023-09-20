#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"


int main(int argc, char* argv[]){
	int pid_child, pid_parent; 	// 父子进程pid
	int p1[2], p2[2];			// 两个管道
    char buf[4];				//传输的东西
    long len = sizeof(buf);		//传输东西的长度
	pipe(p1);					// 创建管道，父写子读
	pipe(p2);					// 创建管道，子写父读
	int child_status = 0;
	if((pid_child=fork())<0){
		printf("FORK ERROR!!!\n");
		exit(1);
	}
	else if(pid_child == 0){
		/* Child */
		pid_child = getpid();
		close(p1[1]);
		close(p2[0]);

		if(read(p1[0], buf, len) != len){
			printf("CHILD READ ERROR!!!\n");
			exit(1);
		}
		printf("%d: received %s\n", pid_child, buf);
		close(p1[0]);

		strcpy(buf, "pong");
		if(write(p2[1], buf, len) != len){
			printf("CHILD READ ERROR!!!\n");
			exit(1);
		}
		close(p2[1]);
		exit(0);
	}
	else{
		/* Parent */
		pid_parent = getpid();
		close(p1[0]);
		close(p2[1]);

		strcpy(buf, "ping");
		if(write(p1[1], buf, len) != len){
			printf("CHILD READ ERROR!!!\n");
			exit(1);
		}
		close(p1[1]);

		if(read(p2[0], buf, len) != len){
			printf("CHILD READ ERROR!!!\n");
			exit(1);
		}
		printf("%d: received %s\n", pid_parent, buf);
		close(p2[0]);
		
		wait(&child_status);
		exit(0);
	}
	exit(0);	// 进程退出
}