#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

// 获取文件名
char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  buf[strlen(p)] = 0;											// 末尾补0
  return buf;
}

void find(char* path, char* file_name){
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;
	// 得到该路径的文件描述符
	if ((fd = open(path, 0)) < 0) {
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}
	// 得到文件状态st
	if (fstat(fd, &st) < 0) {
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch (st.type) {
		case T_FILE:
			// 该st有可能是一个文件
			if((strcmp(fmtname(path), file_name)) == 0)
				printf("%s\n", path);
			break;
	
		case T_DIR:
			// 该st有可能是一个文件夹
			if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {	// 路径长度爆了
				printf("find: path too long\n");
				break;
			}
			strcpy(buf, path);									
			p = buf + strlen(buf);								
			*p++ = '/'; 										// buf="path/"，且p指向其后一个位置
			while (read(fd, &de, sizeof(de)) == sizeof(de)) {	// 得到目录状态de，遍历该路径下每一个文件
				if (de.inum == 0) continue;
				if (strcmp(de.name, ".") == 0) continue;		// .跳过递归
				if (strcmp(de.name, "..") == 0) continue;		// ..跳过递归
				memmove(p, de.name, DIRSIZ);					// de.name即文件名移到路径后面，buf="path/de.name"
				p[DIRSIZ] = 0;									// 末尾补0
				find(buf, file_name);							// 递归
			}
			break;
	}
	close(fd);
}

int main(int argc, char* argv[]){
	if (argc != 3) {
		printf("FIND NEED RIGHT ARGUMENTS!!!\n");
		exit(1);
	}
	char* path = argv[1];
	char* file_name = argv[2];
	find(path, file_name);
	exit(0);
}