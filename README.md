# xv6상에 Continuous File System 구현하기

## 개요
 xv6의 파일 시스템은 inode 구조체에 파일의 디스크 상 주소를 저장할 때 12개의 direct block과 1개의 indirect block을 사용한다.
xv6 파일 시스템이 하나의 블록 크기를 512바이트로 정의하기 때문에 기존의 xv6 파일 시스템의 최대 파일 크기는 512*12 + 512*(512/4) = 71680 Byte이다.
해당 프로젝트는 기존 xv6의 파일 시스템과 달리 continuous sector 기반의 파일 시스템을 구현하는 것을 목표로 한다.
Continuous File System은 inode 구조체 direct block의 4byte를 디스크 상의 주소를 저장하는데 모두 사용하지 않는다.
direct block의 4byte 중 3byte만을 디스크 상의 주소를 저장하는데 사용하고 나머지 1byte는 해당 주소부터 파일이 연속적으로 저장된 sector의 개수를 저장한다.
쉽게 표현하면 12개의 각 direct block마다 4byte를 (디스크 상의 주소 : 3B, 연속적으로 저장된 섹터의 길이 : 1B)로 표현하여 최대 12*512*255 = 1566720 Byte의 파일을 저장할 수 있는 파일 시스템이다.
해당 프로젝트는 위에서 언급한 내용과 같이 inode direct block에 디스크 상의 파일 위치를 Continuous Sector로 표현하고, 이를 기반으로 파일의 create, read, write, delete를 수행하는 파일 시스템을 구현하는 것을 목표로 한다.
<br><br><br>

## 기존 파일 시스템 파일 Create, Write, Read, Delete 분석
1. 파일 Create
기존 파일 시스템의 파일 생성 과정은 다음과 같다.<br>
![image](https://user-images.githubusercontent.com/64363668/235954143-8a02200b-7036-4d17-af11-3eea70655dad.jpeg)
<br>파일을 생성할 때 open 시스템 콜 호출 시 O_CREATE flag를 인자로 넣어 파일을 생성할 수 있고 flag를 처리하는 sysfile.c의 sys_open()함수에서 create() 함수를 호출한다. 이 때 open(), O_CREATE를 통해 생성하는 파일은 일반 파일이므로 sysfile.c 내부적으로 create(path, T_FILE, 0, 0); 을 통해 create의 인자로 T_FILE을 넣어 생성하는 파일의 타입을 결정하는 것을 확인할 수 있다.<br><br>

2. 파일 Write
기존 파일 시스템의 write 수행 과정은 다음과 같다.<br>
![image](https://user-images.githubusercontent.com/64363668/235954375-deb1c5ce-1c30-43d4-b969-00af31361d79.jpeg)
<br> 파일에 쓰기를 수행하기 위해 write() 시스템 콜이 호출되면 sysfile.c의 sys_file() 함수가 호출된다. sys_file() 함수 내에서 file.c 파일의 filewrite() 함수를 호출하는데 이 때 사용되는 filewrite() 함수는 인자로 들어온 addr 포인터의 내용을 n만큼 f에 write하는 함수이다. filewrite() 함수는 fs.c 파일의 writei() 함수를 호출하여 write를 하고 inode를 수정한다. writei() 함수는 인자로 받은 파일의 inode를 확인하고 offset 연산을 통해 쓰고자 하는 데이터를 디스크에 write하는데 이 때 디스크 상에 빈 block을 찾기 위해 bread() 함수와 bmap() 함수를 사용한다. bread() 함수는 두 번째 인자로 들어온 디스크 상의 블록 번호에 있는 내용을 buf형태로 읽어오는 함수이고 bmap()은 두 번째 인자로 들어온 파일 상의 블록 번호를 실제 디스크 상의 블록 번호로 변환하여 리턴해주는 함수이다. 이 때 첫 번째 인자로 받은 inode의 direct block을 확인하여 두 번째 인자인 파일 상의 블록 번호가 저장된 디스크 상의 블록 번호로 변환해주는데 write할 내용은 아직 디스크 상에 할당되지 않았으므로 bmap() 함수 내부에서 데이터 블록을 할당을 한 후 해당 블록의 번호를 리턴해줘야 한다. bmap() 함수 내부에서 디스크 상의 비어 있는 데이터 블록을 찾을 때 balloc()이라는 함수를 호출하는데 balloc() 함수는 파일 시스템의 data bitmap을 순차적으로 확인하며 비어 있는 data block의 블록 번호를 찾고 리턴하는 함수이다. 
write를 수행할 때 bmap() 함수와 balloc() 함수의 역할이 중추적이라고 할 수 있다.<br>
 CS 기반 파일의 write 과정도 유사하다. writei() 함수에서 bmap() 함수를 호출하는 과정까지는 기존 파일 시스템과 일치한다. 하지만 CS 기반 파일 시스템은 bmap()을 호출한 뒤 인자로 받은 inode의 타입을 확인하여 ip->type == T_CS일 경우 따로 구현한 cs_bmap() 함수를 호출하여 direct block을 다른 방식으로 할당하도록 구현하였다.
cs_bmap() 함수의 수도 코드는 다음과 같다.
>static uint cs_bmap(struct unode *ip, uint bn){<br>
>  // 1. read 연산 시 -> bn이 이미 direct block에 할당되어 있는 블록이라면<br>
>  // 2. write 연산 시 -> balloc을 통해 비어 있는 block을 새로 할당 받고(newblk) direct block에 반영해야 함<br>
>  // 2-1. direct block에 아무것도 할당되지 않았을 경우<br>
>  // 2-2. newblk가 direct block에 할당되어 있는 블록과 연속되는 경우<br>
>  // 2-3. newblk가 direct block에 할당되어 있는 블록과 연속되지 않는 경우<br>
>}<br>

write 연산만 고려할 때 newblk = balloc()을 통해 비어 있는 디스크 상의 블록을 할당 받고 해당 블록이 기존에 inode의 direct block 중 연속되는 블록인지 확인해야 한다. 만약 연속된다면 direct block에서 연속되는 길이를 나타내는 하위 1B에 1을 더해주어 연속됨을 표현해줄 수 있다. 만약 하위 1B가 255일 경우 더 이상 연속되는 블록의 길이를 표현해줄 수 없기 때문에 다음 direct block으로 넘어가야 한다.
다른 경우로 newblk가 inode의 direct block과 연속되지 않는다면 새로운 direct block을 할당해주어야 한다. 만약 새로 할당할 수 있는 direct block이 존재하지 않는다면 에러 메시지를 띄우도록 설계하였다.
위와 같은 과정을 통해 CS 기반 파일의 write를 구현할 수 있다.<br><br>



3. 파일 Read
기존 파일 시스템의 Read 수행 과정은 다음과 같다.<br>
![image](https://user-images.githubusercontent.com/64363668/235954529-89324696-6f3e-4c57-b6d3-6f461fcb9192.jpeg)
<br>기존 파일 시스템의 read 과정은 write 과정과 거의 유사하다. read() 시스템 콜 호출 시 sysfile.c의 sys_read() 함수가 호출되고 그 내부에서 file.c 파일의 fileread() 함수가 호출된다. fileread() 함수는 fs.c 파일의 readi() 함수를 호출하여 파일의 내용을 읽는데 readi() 함수는 내부에서 bread()함수와 bmap() 함수를 통해 read할 데이터의 블록 위치를 파악한다. 자세한 내용은 write 과정과 유사하다. <br><br>


4. 파일 Delete
기존 파일 시스템의 파일 삭제 과정은 다음과 같다.<br>
![image](https://user-images.githubusercontent.com/64363668/235954745-8c786281-8c7e-4074-ac5d-fb2d734cbb4b.jpeg)
<br>xv6의 쉘 프롬프트에서 rm 명령어를 사용하면 rm.c 파일이 실행되고 rm.c파일 내부에서 unlink() 시스템 콜이 호출된다. unlink() 시스템 콜 호출 시 sysfile.c의 sys_unlink()를 수행하게 되고 unlink를 수행하게 되는데 sys_unlink() 내부에서 link가 하나인 파일을 unlink하는 경우 할당된 메모리와 inode를 해제한다. 이 때 fs.c에 위치한 iunlockput()이라는 파일을 호출하고 iunlockput()은 iput()함수를 호출한다. iput() 함수는 인자로 받은 inode를 참조하는 link가 하나일 경우 itrunc() 함수를 호출하여 inode를 해제한다.
itrunc() 함수에서는 인자로 받은 inode 포인터가 가리키는 inode 내부 값을 모두 초기화하고 inode의 direct block이 가리키는 디스크 상의 메모리도 모두 해제한다.

## CS 기반 파일 시스템 파일 Create, Write, Read, Delete

## 실행 결과
