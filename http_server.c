#define BUF_SIZE 1000
#define HEADER_FMT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Disposition: inline\nContent-Type: %s\n\n"

#define NOT_FOUND_CONTENT "<h1>404 Not Found</h1>\n"
#define SERVER_ERROR_CONTENT "<h1>500 Internal Server Error</h1>\n"

#include <stdio.h>  //기본 입출력
#include <string.h> //문자열 지원 헤더
#include <stdlib.h> //표준 유틸리티 지원 헤더
#include <unistd.h> //POSIX 운영체제 API 액세스 제공 헤더 파일
#include <fcntl.h>  //POSIX 운영체제 중 파일을 열기/잠금 및 다른 작업 가능하게 해주는 헤더
#include <signal.h> //software interrupt인 signal을 다루는 헤더
#include <sys/types.h>  //OS에서 사용하는 자료형에 관한 정보들을 담는 헤더
#include <sys/stat.h>   //파일의 상태에 대한 정보를 얻거나 설정하는 헤더
#include <sys/socket.h> //소켓 프로그램에 필요한 헤더
#include <arpa/inet.h>  //주소 변환 기능을 사용할 때 사용하는 헤더

/*
    @func   생성된 소켓 lsock(sd)에 주소를 부여하는 함수
    @return bind() return value
*/
int bind_lsock(int lsock, int port) {
    struct sockaddr_in sin;     /*
                                    sockaddr 안에는 2가지 멤버가 있음
                                        u_short(unsigned short) sa_family: 주소 체계를 구분하기 위한 변수. 2bytes
                                        char sa_data[14]: 실제 주소를 저장하기 위한 변수. 14bytes.
                                
                                    sockaddr_in 안에는 4가지 멤버가 있음
                                    sockaddr를 그대로 사용할 경우, sa_data 안에 ip주소와 Port 번호가 조합되어있어 읽고 쓰기 불편
                                    sa_family가 AF_INET일경우 sockaddr 구조체를 사용하지 않고 sockaddr_in 구조체를 사용
                                    여기서 ip주소는 ipv4 주소체계를 사용
                                        short sin_family: 주소 체계. 반드시 AF_INET로 설정
                                        u_short sin_port: 16 비트 포트 번호, network byte order. 1024 이하의 포트번호는 previleged port. 권한을 가진 프로세스만 이용 가능
                                        struct in_addr sin_addr: 32비트 ip 주소
                                        char sin_zero[8]        :전체 크기를 16비트로 맞추기 위한 더미
                                    
                                    -struct in_addr
                                        u_long s_addr: 32비트 ip주소를 저장할 구조체, network byte order
                                */
    sin.sin_family = AF_INET;   //반드시 AF_INET 으로 설정
    sin.sin_addr.s_addr = htonl(INADDR_ANY);    //32비트의 Host-Byte-Order로부터 TCP/IP에서 사용되는 Network-Byte-Order로 변환하는 함수
                                                //INADDR_ANY: 이 컴퓨터에 존재하는 랜카드 중 사용가능한 랜카드의 ip주소를 사용하라는 의미
    sin.sin_port = htons(port); //포트 번호를 호스트 바이트 순서에서 네트워크 바이트 순서로(host to network short)

    return bind(lsock, (struct sockaddr*)&sin, sizeof(sin));    //소켓에 주소를 할당해주는 함수
}

//HTTP 관련 함수

/*
    HTTP request는 브라우저에서 보내주므로, 서버의 책임은 이 요청을 읽고 적절한 응답을 보내는 것이다. 그러므로:
        -요청에서 Path "/"을 읽어 적절한 리소스를 반환해야 한다. "/hello.html"이라면 hello.html 파일 내용으로 응답해야 한다.
        -응답에 적절한 상태 코드(200, OK)와 헤더를 적어줘야 한다. 필수적으로 포함해야할 헤더는 Content-Length, Content-Type
        Content-Length에는 리소스 파일의 크기를, Content-Type에는 파일 확장자에 따른 MIME Type 값을 적어준다.
        -Content-Length, Content-Type은 중요하다. Content-Length는 브라우저가 헤더 다음 몇 바이트를 읽어야 하는 지 알려준다.
        -Content-Type은 body가 어떤 타입인지, 브라우저에서 어떻게 보여줘야 하는지를 알려준다.
        예를 들어, HTML파일을 보내주는데 Content-Type를 text/plain으로 알려주면, 브라우저는 단순히 HTML 코드 그대로를 화면에 보여준다.
*/

/*
    @func   주어진 param들을 이용해 HTTP 헤더 작성
            상태 코드, 헤더 내용 등을 주어진 포인터에 채운다. #define 부분에 헤더 포맷 매크로를 정의했다. 여기서 사용.
*/
void fill_header(char* header, int status, long len, char* type) {
    char status_text[40];
    switch (status) {
        case 200:   //정상적인 통신
            strcpy(status_text, "OK");  break;
        case 404:   //404 not found error
            strcpy(status_text, "Not Found");   break;
        case 500:   //Internal server Error
        default:
            strcpy(status_text, "Internal Server Error");   break;
    }
    sprintf(header, HEADER_FMT, status, status_text, len, type);    //sprintf: 출력 값을 변수에 저장 후 출력
                                                                    //header에 HEADER_FMT값과 여러 변수 값을 담은 후 저장하고 출력한다.
}

/*
    @func   uri에서 content type을 찾는다
            파일의 확장자를 참조하여 적절한 Content Type 값을 주어진 포인터에 채운다.
*/
void find_mime(char* ct_type, char* uri) {
    char* ext = strrchr(uri, '.');  //문자열에서 특정 문자가 있는 위치를 뒤에서 부터 찾는 함수(포인터 반환) 없다면 NULL 반환
    if (!strcmp(ext, ".html"))
        strcpy(ct_type, "text/html");
    else if (!strcmp(ext, ".gif"))
        strcpy(ct_type, "image/gif");
    else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
        strcpy(ct_type, "image/jpeg");
    else if (!strcmp(ext, ".mp3"))
        strcpy(ct_type, "audio/mp3");
    else if (!strcmp(ext, ".pdf"))
        strcpy(ct_type, "application/pdf");
    else strcpy(ct_type, "text/plain");
}

/*
    @func   404 not found error handler
            상태코드 404로 응답할 때 사용하는 함수
*/
void handle_404(int asock) {
    char header[BUF_SIZE];
    fill_header(header, 404, sizeof(NOT_FOUND_CONTENT), "text/html");

    write(asock, header, strlen(header));           //write(int fd:파일 디스크립터, void* buf:버퍼, size_t nbytes:읽어들일 데이터 최대 길이)
                                                    //파일 디스크립터 fd를 열고 그 내용을 버퍼 buf에 넣는다.
    write(asock, NOT_FOUND_CONTENT, sizeof(NOT_FOUND_CONTENT));
}

/*
    @func   500 internal server error handler
            상태코드 500으로 응답할 때 사용하는 함수
*/
void handle_500(int asock) {
    char header[BUF_SIZE];
    fill_header(header, 500, sizeof(SERVER_ERROR_CONTENT), "text/html");
}

/*
    @func   main http handler; try to open and send requested resource
            calls error handler on failure
            요청된 파일을 읽으려고 하며, 파일 접근에 성공하면 상태 코드 200으로 파일의 내용을 정상적으로 보낸다.
            도중에 실패하면 handle_404(), handle_500()을 호출한다.
*/
void http_handler(int asock) {
    char header[BUF_SIZE];
    char buf[BUF_SIZE];

    if (read(asock, buf, BUF_SIZE) < 0) {   //read 실패
        perror("[ERR] Failed to read request.\n");
        handle_500(asock);
        return;
    }

    char* method = strtok(buf, " ");    //string을 공백(공백을 인자로 줬기 때문)을 기준으로 자른다.(str tokenize)
    char* uri = strtok(NULL, " ");      //인자에 NULL 값을 주면 잘려진 부분 이후로 다시 자르기 시작한다. strtok가 static 변수를 가지고 있어서 잘려진 위치의 포인터를 기억하기 때문.
    if (method == NULL || uri == NULL) {
        perror("[ERR] Failed to identify method, URI.\n");
        handle_500(asock);
        return;
    }

    printf("[INFO] Handling Request: method = %s, URI = %s\n", method, uri);

    char safe_uri[BUF_SIZE];
    char* local_uri;
    struct stat st; /*
                        stat 구조체의 st_mode 변수가 의미가 있다.
                        mode_t st_mode  //파일의 모드를 다룸
                        st_mode에 따라서 파일의 종류와 퍼미션을 알 수 있다.

                            S_SIREG(m): is it a regular file?
                            S_ISDIR(m): directory?
                            S_ISCHR(m): character device?
                            S_ISBLK(m): block device?
                            S_ISFIFO(m): FIFO (named pipe)?
                            S_ISLINK(m): symbolic link? (Not in POSIX.1-1996)
                            S_ISSOCK(m): socket? (Not in POSIX.1-1996.)

                        The following flags are defined for the st_mode field

                            S_IFMT          bit mask for the file type bit fields
                            S_IFSOCKET      socket
                            S_IFLINK        symbolic lik
                            S_IFREG         regular file
                            S_IFBLK         block device
                            S_IFDIR         directory
                            S_IFCHR         character device
                            S_IFIFO         FIFO
                            S_ISUID         set UID bit
                            S_ISGID         set-group-ID bit
                            S_ISVTX         sticky bit
                            S_IRWXU         mask for file owner permissions
                            S_IRUSR         owner has read permission
                            S_IWUSR         owner has write permission
                            S_IXUSR         owner has execute permission
                            S_IRWXG         mask for group permissions
                            S_IRGRP         group has read permission
                            S_IWGWP         group has write permission
                            S_IXGRP         group has execute permission
                            S_IRWXO         mask for permissions for others (not in group)
                            S_IROTH         others have read permission
                            S_IWOTH         others have write permission

                            S_IXOTH         others have execute permission
                    */
    strcpy(safe_uri, uri);
    if (!strcmp(safe_uri, "/")) strcpy(safe_uri, "/index.html");

    local_uri = safe_uri + 1;
    if (stat(local_uri, &st) < 0) { //stat함수가 오류가 나면 -1 반환. 파일의 상태 및 정보를 확인하는 함수이다. 오류 내용은 전역변수 errno 전역변수에 저장되어 있음
        perror("[WARN] No file found matching URI.\n");
        handle_404(asock);
    }

    int fd = open(local_uri, O_RDONLY);
    if (fd < 0) {
        perror("[ERR] Failed to open file.\n");
        handle_500(asock);
        return;
    }

    int ct_len = st.st_size;
    char ct_type[40];
    find_mime(ct_type, local_uri);
    fill_header(header, 200, ct_len, ct_type);
    write(asock, header, strlen(header));

    int cnt;
    while ((cnt = read(fd, buf, BUF_SIZE)) > 0)
        write(asock, buf, cnt);
}


int main(int argc, char** argv) {
    int port, pid;
    int lsock, asock;

    struct sockaddr_in remote_sin;
    socklen_t remote_sin_len;

    if (argc < 2) {     //사용법 안내
        printf("Usage: \n");
        printf("\t%s {port}: runs mini HTTP server.\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);
    printf("[INFO] The Server will listen to port: %d.\n", port);

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) {
        perror("[ERR] failed to create lsock.\n");
        exit(1);
    }

    if (bind_lsock(lsock, port) < 0) {
        perror("[ERR] failed to bind lsock.\n");
        exit(1);
    }

    if (listen(lsock, 10) < 0) {
        perror("[ERR] failed to listen lsock.\n");
        exit(1);
    }

    //to handle zombie process
    signal(SIGCHLD, SIG_IGN);   /*
                                    SIGCHLD: fork했을 때 child process가 끝나면 나오는 신호
                                    SIG_IGN: 시그널 무시하라는 명령

                                    -클라이언트 측에서 FIN을 전송하여 4HSK를 통해 정상적으로 자식이 종료된 후
                                    부모 소켓에게 SIGCHLD신확 전달. accpet가 블록된다.
                                    일시 정지된 시스템 콜을 자동으로 재시작하지 않는 문제가 발생한다.
                                    이를 slow system call이라고 한다.(영원히 일시 중지 될 수 있는 시스템콜)

                                    이 함수를 사용하면 좀비 소켓을 처리할 수 있다.
                                    waitpid를 하는 signal handler를 사용해도 무방.
                                */

    while (1) {
        printf("[INFO] waiting...\n");
        asock = accept(lsock, (struct sockaddr*)&remote_sin, &remote_sin_len);
        if (asock < 0) {
            perror("[ERR] failed to accept.\n");
            continue;
        }

        pid = fork();
        if (pid == 0) {
            close(lsock);
            http_handler(asock);
            close(asock);
            exit(0);
        }

        if (pid != 0)   {close(asock);}
        if (pid < 0)    {perror("[ERR] failed to fork.\n");}
    }                          
}