# xv6상에 Continuous File System 구현하기

## 개요
 xv6의 파일 시스템은 inode 구조체에 파일의 디스크 상 주소를 저장할 때 12개의 direct block과 1개의 indirect block을 사용한다.<br>
xv6 파일 시스템이 하나의 블록 크기를 512바이트로 정의하기 때문에 기존의 xv6 파일 시스템의 최대 파일 크기는 $$512 \times 12 + 512 \times (512/4) = 71680$$ Byte이다.<br>
해당 프로젝트는 기존 xv6의 파일 시스템과 달리 continuous sector 기반의 파일 시스템을 구현하는 것을 목표로 한다.<br>
Continuous File System은 inode 구조체 direct block의 4byte를 디스크 상의 주소를 저장하는데 모두 사용하지 않는다.<br>
direct block의 4byte 중 3byte만을 디스크 상의 주소를 저장하는데 사용하고 나머지 1byte는 해당 주소부터 파일이 연속적으로 저장된 sector의 개수를 저장한다.<br>
쉽게 표현하면 12개의 각 direct block마다 4byte를 (디스크 상의 주소 : 3B, 연속적으로 저장된 섹터의 길이 : 1B)로 표현하여 최대 $$12 \times 512 \times 255 = 1566720$$ Byte의 파일을 저장할 수 있는 파일 시스템이다.<br>
해당 프로젝트는 위에서 언급한 내용과 같이 inode direct block에 디스크 상의 파일 위치를 Continuous Sector로 표현하고, 이를 기반으로 파일의 create, read, write, delete를 수행하는 파일 시스템을 구현하는 것을 목표로 한다.
<br><br><br>

## 기존 파일 시스템 파일 Create, Write, Read, Delete 분석
#### 1. 파일 Create
기존 파일 시스템의 파일 생성 과정은 다음과 같다.<br>

![image](https://user-images.githubusercontent.com/64363668/235954143-8a02200b-7036-4d17-af11-3eea70655dad.jpeg)

<br>파일을 생성할 때 open 시스템 콜 호출 시 O_CREATE flag를 인자로 넣어 파일을 생성할 수 있고 flag를 처리하는 sysfile.c의 sys_open()함수에서 create() 함수를 호출한다. 이 때 open(), O_CREATE를 통해 생성하는 파일은 일반 파일이므로 sysfile.c 내부적으로 create(path, T_FILE, 0, 0); 을 통해 create의 인자로 T_FILE을 넣어 생성하는 파일의 타입을 결정하는 것을 확인할 수 있다.<br><br>

#### 2. 파일 Write
기존 파일 시스템의 write 수행 과정은 다음과 같다.<br>

![image](https://user-images.githubusercontent.com/64363668/235954375-deb1c5ce-1c30-43d4-b969-00af31361d79.jpeg)

<br> 파일에 쓰기를 수행하기 위해 write() 시스템 콜이 호출되면 sysfile.c의 sys_file() 함수가 호출된다. sys_file() 함수 내에서 file.c 파일의 filewrite() 함수를 호출하는데 이 때 사용되는 filewrite() 함수는 인자로 들어온 addr 포인터의 내용을 n만큼 f에 write하는 함수이다.<br> filewrite() 함수는 fs.c 파일의 writei() 함수를 호출하여 write를 수행하고 쓰기가 수행된 파일의 inode를 수정한다. writei() 함수는 인자로 받은 파일의 inode를 확인하고 offset 연산을 통해 쓰고자 하는 데이터를 디스크에 write하는데 이 때 디스크 상에 빈 block을 찾기 위해 bread() 함수와 bmap() 함수를 사용한다.<br> bread() 함수는 두 번째 인자로 들어온 디스크 상의 블록 번호에 있는 내용을 buf형태로 읽어오는 함수이고 bmap()은 두 번째 인자로 들어온 파일 상의 블록 번호를 실제 디스크 상의 블록 번호로 변환하여 리턴해주는 함수이다. 이 때 첫 번째 인자로 받은 inode의 direct block을 확인하여 두 번째 인자인 파일 상의 블록 번호가 저장된 디스크 상의 블록 번호로 변환해주는데 write할 내용은 아직 디스크 상에 할당되지 않았으므로 bmap() 함수 내부에서 데이터 블록을 할당을 한 후 해당 블록의 번호를 리턴해줘야 한다. bmap() 함수 내부에서 디스크 상의 비어 있는 데이터 블록을 찾을 때 balloc()이라는 함수를 호출하는데 balloc() 함수는 파일 시스템의 data bitmap을 순차적으로 확인하며 비어 있는 data block의 블록 번호를 찾고 리턴하는 함수이다. 
write를 수행할 때 데이터를 저장할 디스크 상에 빈 메모리를 찾는데 bmap() 함수와 balloc() 함수의 역할이 중추적이라고 할 수 있다.<br><br>


#### 3. 파일 Read
기존 파일 시스템의 Read 수행 과정은 다음과 같다.<br>

![image](https://user-images.githubusercontent.com/64363668/235954529-89324696-6f3e-4c57-b6d3-6f461fcb9192.jpeg)

<br>기존 파일 시스템의 read 과정은 write 과정과 거의 유사하다. read() 시스템 콜 호출 시 sysfile.c의 sys_read() 함수가 호출되고 그 내부에서 file.c 파일의 fileread() 함수가 호출된다. fileread() 함수는 fs.c 파일의 readi() 함수를 호출하여 파일의 내용을 읽는데 readi() 함수는 내부에서 bread()함수와 bmap() 함수를 통해 read할 데이터의 블록 위치를 파악한다. 자세한 내용은 write 과정과 유사하다. <br><br>


#### 4. 파일 Delete
기존 파일 시스템의 파일 삭제 과정은 다음과 같다.<br>

![image](https://user-images.githubusercontent.com/64363668/235954745-8c786281-8c7e-4074-ac5d-fb2d734cbb4b.jpeg)

<br>xv6의 쉘 프롬프트에서 rm 명령어를 사용하면 rm.c 파일이 실행되고 rm.c파일 내부에서 unlink() 시스템 콜이 호출된다. unlink() 시스템 콜 호출 시 sysfile.c의 sys_unlink()를 수행하게 되고 unlink를 수행하게 되는데 sys_unlink() 내부에서 link가 하나인 파일을 unlink하는 경우 할당된 메모리와 inode를 해제한다. 이 때 fs.c에 위치한 iunlockput()이라는 파일을 호출하고 iunlockput()은 iput()함수를 호출한다. iput() 함수는 인자로 받은 inode를 참조하는 link가 하나일 경우 itrunc() 함수를 호출하여 inode를 해제한다.
itrunc() 함수에서는 인자로 받은 inode 포인터가 가리키는 inode 내부 값을 모두 초기화하고 inode의 direct block이 가리키는 디스크 상의 메모리도 모두 해제한다.<br><br>

## CS 기반 파일 시스템 파일 Create, Write, Read, Delete

#### 1. CS 기반 파일 Create
CS 기반 파일을 생성하기 위해서는 open()시 O_CS 플래그를 받아 sysfile.c 내부에서 O_CS 플래그를 검사하여 create를 호출할 때 create(path, T_CS, 0, 0); 을 통해 T_CS 타입의 파일을 생성할 수 있다.<br><br>


#### 2. CS 기반 파일 Write
writei() 함수에서 bmap() 함수를 호출하는 과정까지는 기존 파일 시스템과 일치한다. 하지만 CS 기반 파일 시스템은 bmap()을 호출한 뒤 인자로 받은 inode의 타입을 확인하여 ip->type == T_CS일 경우 cs_bmap() 함수를 호출하여 direct block을 다른 방식으로 할당하도록 구현하였다.<br>
cs_bmap() 함수의 수도 코드는 다음과 같다.<br>
~~~
static uint cs_bmap(struct unode *ip, uint bn){
  // 1. read 연산 시 -> bn이 이미 direct block에 할당되어 있는 블록이라면
  // 2. write 연산 시 -> balloc을 통해 비어 있는 block(newblk)을 새로 할당 받고 direct block에 저장해야 함
  // 2-1. direct block에 아무것도 할당되지 않았을 경우
  // 2-2. newblk가 direct block에 할당되어 있는 블록과 연속되는 경우
  // 2-3. newblk가 direct block에 할당되어 있는 블록과 연속되지 않는 경우
}
~~~
write 연산만 고려할 때 newblk = balloc()을 통해 비어 있는 디스크 상의 블록을 할당 받고 해당 블록이 기존에 inode의 direct block 중 연속되는 블록인지 확인해야 한다. 만약 연속된다면 direct block에서 연속되는 길이를 나타내는 하위 1B에 1을 더해주어 연속됨을 표현해줄 수 있다. 만약 하위 1B가 255일 경우 더 이상 연속되는 블록의 길이를 표현해줄 수 없기 때문에 다음 direct block으로 넘어가야 한다.<br>
다른 경우로 newblk가 inode의 direct block과 연속되지 않는다면 새로운 direct block을 할당해주어야 한다. 만약 새로 할당할 수 있는 direct block이 존재하지 않는다면 에러 메시지를 띄우도록 설계하였다.<br>
위와 같은 과정을 통해 CS 기반 파일의 write를 구현할 수 있다.<br><br>



#### 3. CS 기반 파일 Read
CS 기반 파일을 read할 때도 write와 같이 bmap() 함수 내에서 ip->type == T_CS인 경우에 cs_bmap() 함수를 호출하여 데이터의 디스크 상 블록 번호를 리턴하도록 설계하였다.<br>
cs_bmap에서 read할 데이터의 블록 번호를 찾기 위해서는 inode의 direct block을 확인해야 한다.<br>
inode의 direct block을 탐색하며 cs_bmap()의 두 번째 인자로 받은 파일 상의 블록 번호가 디스크 상의 블록 번호를 찾아야 하는데 이때 shift연산과 논리 연산을 통해 direct block의 (번호, 길이)를 분리하여 찾을 수 있다. 하위 1B를 매크로로 정의한 ONEB 255와 & 연산을 통해 분리하고 direct block을 >>8하여 상위 3B도 구할 수 있다. 이렇게 구한 블록의 시작 번호와 연속된 길이를 계산하여 두 번째 인자로 받은 파일의 블록 번호에 매칭되는 디스크 상의 블록 번호를 리턴하도록 구현하였다.<br><br>


#### 4. CS 기반 파일 Delete
CS 기반 파일 시스템과 기존 파일 시스템의 삭제는 direct block 해제 방식이라는 차이점이 존재한다. CS 기반의 파일의 direct block은 (번호, 길이) 형태로 저장되어 있기 때문에 파일의 모든 메모리를 해제하기 위해서는 모든 direct block에 저장된 ‘번호 ~ 번호+길이’까지의 메모리를 모두 해제해주어야 한다.<br>
따라서 trunc() 함수에서 ip->type == T_CS인 경우 위의 방식으로 inode - direct block에 할당된 메모리를 해제하는 함수인 cs_itrunc()를 호출하도록 구현하였다.<br><br>


## 실행 결과
실행 결과를 효율적으로 확인하기 위해 경우를 나누어 보았다.
1) 파일을 연속적으로 할당할 경우<br>
2) 파일을 할당하던 중 중간에 다른 파일을 할당한 후 다시 이어서 할당한 경우<br>
3) 파일시스템의 블록의 수가 1000개를 넘어갈 경우<br>
4) CS 파일에서 최대 할당 바이트 수인 1436160(11 * 255 * 512) byte를 넘게 할당할 경우<br>
5) CS 파일이 direct block의 개수를 초과하여 할당을 시도할 경우<br>
6) 파일을 삭제하였을 경우<br>
7) wc 명령어 결과<br>
<br>

#### 1) 파일을 연속적으로 할당할 경우

![image](https://user-images.githubusercontent.com/64363668/236090987-11ff8131-af0d-4d38-bbb2-c5e9cdc3f57a.png)

<br>파일의 크기가 133120 byte(260개의 block)인 파일을 연속적으로 할당할 경우의 실행 결과이다. 한 번에 1024B씩 130번 write한 경우이다.<br>
#### 2) 파일을 할당하던 중 중간에 다른 파일을 할당한 후 다시 이어서 할당한 경우<br>

![image](https://user-images.githubusercontent.com/64363668/236091039-d04dfb09-9de7-40a7-918e-bce143f69864.png)

<br>파일의 크기가 65000 byte(127개의 block)인 파일을 연속적으로 할당할 경우의 실행 결과이다. 한 번에 500B씩 130번 write 한 경우이다.<br>

![image](https://user-images.githubusercontent.com/64363668/236091074-2130006a-e3f1-474d-9c46-51e94fe63271.png)

<br>파일을 할당하던 중 중간에 다른 파일을 할당한 후 다시 이어서 할당한 경우
<br>파일의 크기가 133120 byte(260개의 block)인 파일을 연속적으로 할당하던 중 102번째 block까지 할당하고 중간에 다른 파일을 쓰다가 다시 이어서 할당을 시작한 경우의 실행 결과이다. 한 번에 1024B씩 130번 write하는 경우인데 52번째 block을 write한 후 새로운 일반 파일에 2048B를 write하고 다시 이어서 write하는 경우이다.<br>

![image](https://user-images.githubusercontent.com/64363668/236091167-f51d3789-94a8-4370-869f-4553071a65a9.png)

<br>파일의 크기가 65000 byte(127개의 block)인 파일을 연속적으로 할당하던 중 50번째 block까지 할당하고 중간에 다른 파일을 쓰다가 다시 이어서 할당을 시작한 경우의 실행 결과이다. 한 번에 500B씩 130번 write하는 경우인데 50번째 block을 write한 후 새로운 일반 파일에 1000B를 write하고 다시 이어서 write하는 경우이다.<br>

#### 3) 파일시스템의 블록의 수가 1000개를 넘어갈 경우

![image](https://user-images.githubusercontent.com/64363668/236091218-4c1b96c2-0577-477a-8094-538255e8f731.png)

<br>v6 파일시스템의 최대 블록 개수인 1000까지 할당한 경우이다.<br>

![image](https://user-images.githubusercontent.com/64363668/236091241-4bc3b4be-170a-4c9d-8a3f-4e523ecdfbe2.png)

<br>1001번째 블록에 할당을 시도한 경우이다.<br>

#### 4) CS 파일에서 최대 할당 바이트 수인 1436160(11 * 255 * 512) byte를 넘게 할당할 경우

![image](https://user-images.githubusercontent.com/64363668/236091327-d4db5b5f-f1aa-40df-b4f8-d320d461fda6.png)

<br>CS 파일의 최대 할당 바이트 수를 넘어갈 경우이다. 최대 할당 바이트 수를 넘어가기 전에 이미 블록의 개수가 1000을 넘어버려 out of blocks 에러가 처리된다.<br>

#### 5) CS 파일이 direct block의 개수를 초과하여 할당을 시도할 경우

![image](https://user-images.githubusercontent.com/64363668/236091385-7e6c7a06-a741-4a93-b78b-36669cb23df3.png)
![image](https://user-images.githubusercontent.com/64363668/236091406-3f7e5938-e9e6-4493-b43b-6594371161bf.png)

<br>CS 파일을 write하는 도중 중간에 다른 파일을 할당하고 다시 이어서 write를 하면 블록의 연속이 끊겨 다음 direct block에 디스크 블록 (번호, 길이)를 저장하게 된다. 즉 연속이 끊길 경우, 다음 direct block에 작성해야 한다. 2) 결과가 이 사실을 보여준다.<br>
그렇다면 만약 2) 같이 CS 파일을 연속적으로 할당할 때 중간에 다른 파일을 여러 번 작성하여 연속이 여러 번 끊길 경우의 결과는 <그림 8>, <그림 9>과 같다.<br>
한번에 512byte씩 총 130번 write하는 cs파일과 중간에 512byte씩 두 번 write하는 일반 파일들을 생성하는 코드이다.<br>

![image](https://user-images.githubusercontent.com/64363668/236091851-6a2c9198-d319-4e1c-883b-138bbc645a77.png)
![image](https://user-images.githubusercontent.com/64363668/236091891-fe325ecf-3ccc-4085-9844-db9218120dd0.png)
![image](https://user-images.githubusercontent.com/64363668/236091906-4c058423-3fc7-49af-8cf8-d54b0151db6d.png)

<br>그렇다면 direct block의 최대 개수를 넘어가는 경우는 어떻게 될까? 이러한 경우 명세에서 에러 처리를 요구하였고 fs.c의 cs_bmap()함수에서 panic(“out of direct block”)을 발생시키도록 처리하였다. 그 결과는 <그림 10>,  <그림 11>,  <그림 12>과 같다.<br>
write 수행은 위의 상황과 동일하지만 중간에 새로 생성하는 파일을 12개 이상으로 수행하였을 때의 결과이다. CS 파일은 총 12개의 direct block을 사용할 수 있기 때문에 13번째의 direct block에 접근할 때 panic() 메세지가 호출되는 것을 확인할 수 있다.<br>
CS 파일의 write는 파일 “a”를 생성하기 전부터 시작한다.<br>

#### 6) 파일을 삭제하였을 경우

![image](https://user-images.githubusercontent.com/64363668/236092042-fafa844d-4f14-403f-9541-699084e951bf.png)
![image](https://user-images.githubusercontent.com/64363668/236092075-1e06c946-d784-4462-a82e-b9f395a02b3b.png)

<br> ./test 이후 생성된 test_cs와 test_norm 파일이 존재할 때 CS 기반의 파일이 rm 명령어를 통해 정상적으로 삭제되는 지 확인할 수 있다. <그림 13>, <그림 14>의 결과가 삭제 수행의 결과이다.<br>
                                                                                                     
#### 7) wc 명령어 결과

![image](https://user-images.githubusercontent.com/64363668/236092217-c5bd1eed-c3ce-49ad-a46c-47b344f191ca.png)

<br><그림 15>는 ./test를 수행하여 생성된 CS 기반의 파일을 wc 명령어로 읽었을 때의 결과이다.



