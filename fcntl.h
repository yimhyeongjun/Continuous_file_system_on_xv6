#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200

// OS5 : CS 기반 파일을 생성할 때 fcntl.h에 플래그 추가
#define O_CS 	  0x020
