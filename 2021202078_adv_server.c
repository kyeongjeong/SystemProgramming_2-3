///////////////////////////////////////////////////////////////////////////////////////
// File Name    : 2021202078_web_server.c                                            //
// Date         : 2023/05/03                                                         //
// OS           : Ubuntu 16.04.5 Desktop 64bits                                      //
// Author       : Choi Kyeong Jeong                                                  //
// Student ID   : 2021202078                                                         //
// --------------------------------------------------------------------------------- //
// Title        : System Programming Assignment 2-2                                  //
// Descriptions : The results of Assignment 2-2 are transmitted to a web browser     //
//                through a server in response to the client's requests. In this     //
//                case, additional results should be sent depending on the directory //
//                and file clicked.                                                  //
///////////////////////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE
#define FNM_CASEFOLD  0x10
#define MAX_LENGTH 1000
#define URL_LEN 256
#define BUFSIZE 1024
#define PORTNO 44444

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <ctype.h>
#include <fnmatch.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/socket.h>

int request = 0;
int No = 0;
int Port[10];
char IP[10][MAX_LENGTH];
time_t TIME[10];
pid_t PID[10];

void printServerHistory();
void saveConnectHistory(struct sockaddr_in client_addr);
void sendResponse(char* url, char* response_header, int isNotStart, int client_fd);
void listDirFiles(int a_hidden, int l_format, int S_size, int r_reverse, int h_readable, char* filename, FILE *file);
void getAbsolutePath(char *inputPath, char *absolutePath);
void joinPathAndFileName(char* path, char* Apath, char* fileName);
void sortByNameInAscii(char **fileList, int fileNum, int start, int r_reverse);
void printPermissions(mode_t mode, FILE *file);
void printType(struct stat fileStat, FILE *file);
void findColor(char* fileName, char* color);
void printAttributes(struct stat fileStat, FILE *file, int h_readable, char *color);
int compareStringUpper(char* fileName1, char* fileName2);
int writeHTMLFile(char* url);
int isAccesible(char* inputIP, char* response_header, int client_fd);

///////////////////////////////////////////////////////////////////////////////////////
// main                                                                              //
// --------------------------------------------------------------------------------- //
// Input:                                                                            //
// output:                                                                           //
// purpose: Create a socket to ensure smooth communication between the server and    //
//          client. Depending on the client's request, the server sends the          //
//          corresponding results to the web browser, which includes exception       //
//          handling for each error situation.                                       //
///////////////////////////////////////////////////////////////////////////////////////
int main() {
    
    struct sockaddr_in server_addr, client_addr; //서버 및 클라이언트의 주소
    int socket_fd, client_fd; //소켓 및 클라이언트의 file descriptor
    unsigned int len; //클라이언트 주소의 길이
    int opt = 1; //소켓의 옵션 사용 설정
    int isNotStart = 0; //클라이언트의 초기 요청인지 구분

    if((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) { //소켓 생성
        printf("Server : Can't open stream socket\n"); //소켓 생성 실패 시
        return 0; //프로그램 종료
    }

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET; //주소 체계를 IPv4로 설정
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //모든 IP 주소를 허용
    server_addr.sin_port = htons(PORTNO); //포트 번호 설정

    if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { //소켓에 IP 주소와 포트 번호 할당
        printf("Server : Can't bind local address\n"); //소켓 할당 실패 시 
        return 0; //프로그램 종료
    }

    listen(socket_fd, 5); //클라이언트의 연결 요청 대기

    No = 0;
    while(1) {
        
        struct in_addr inet_clinet_address; //클라이언트의 주소
        char response_header[BUFSIZE] = {0, }; //응답 메세지 헤더
        char buf[BUFSIZE] = {0, }; //클라이언트의 요청 메세지
        char tmp[BUFSIZE] = {0, }; //요청 메세지 복사 후 토큰으로 분리하기 위한 변수
        char url[URL_LEN] = {0, }; //클라이언트의 요청 URL 주소
        char method[20] = {0, }; //클라이언트의 HTTP 요청 메소드
        char* tok = NULL; //토큰으로 분리할 문자열 포인터
        pid_t pid;

        len = sizeof(client_addr); //클라이언트의 주소 길이 저장
        client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &len); //클라이언트로부터 요청 받음

        if(client_fd < 0) {
            printf("Server : accept failed\n"); //연결 요청 실패 시
            return 0; //프로그램 종료
        }

        pid = fork();

        if (pid == 0) { // 자식 프로세스

            printf("========= New Client ============\nIP : %s\n Port : %d\n=================================\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // 연결된 클라이언트의 IP 주소 및 포트 번호 출력
            ++request;  

            inet_clinet_address.s_addr = client_addr.sin_addr.s_addr; //클라이언트 주소 정보 저장
            read(client_fd, buf, BUFSIZE); //요청 메세지 read
            strcpy(tmp, buf);

            tok = strtok(tmp, " "); //HTTP 요청 메소드 받음
            strcpy(method, tok); //method에 tok 내용 저장
            if(strcmp(method, "GET") == 0) { //GET 요청인 경우
                tok = strtok(NULL, " "); //요청한 URL 주소 받음
                strcpy(url, tok); //url에 tok 내용 저장
            }

            if(strcmp(url, "/favicon.ico") == 0) //url이 /favicon.ico인 경우 무시
                continue;

            if(isAccesible(inet_ntoa(client_addr.sin_addr), response_header, client_fd) == 0)
                continue;
            
            sendResponse(url, response_header, isNotStart, client_fd); //아닌 경우, response 메세지 입력 및 출력
            saveConnectHistory(client_addr);

            close(client_fd);
            printf("====== Disconnected Client ======\nIP : %s\n Port : %d\n=================================\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); // 연결된 클라이언트의 IP 주소 및 포트 번호 출력  
            
            printf("time(NULL) : %ld\n", time(NULL));
            if (time(NULL) % 10 == 0) 
                printServerHistory();
            exit(0); // 자식 프로세스 종료
        }

        else if (pid < 0) { // 프로세스 생성 실패 시
            printf("Server : fork failed\n");
            continue;
        }

        else {
            int *status;
            close(client_fd);
            wait(status);
        }

        close(client_fd);
        isNotStart = 1; //클라이언트의 초기 요청이 아님을 저장
    }
    close(socket_fd); //socket descriptor close
    return 0; //프로그램 종료
}

int isAccesible(char* inputIP, char* response_header, int client_fd) {

    FILE* file = fopen("accessible.usr", "r");
    char accessIDs[MAX_LENGTH];

    while(fgets(accessIDs, MAX_LENGTH, file) != NULL) {
        
        if(fnmatch(accessIDs, inputIP, 0) == 0)
            return 1;
    }

    char error_message[MAX_LENGTH]; //서버의 에러 응답 메세지
    sprintf(error_message, "<h1>Access denied!</h1><br>"
                           "<h3>Your IP : %s</h3><br>"
                           "you have no permission to access this web server.<br>"
                           "HTTP 403.6 - Forbidden: IP address reject<br>", inputIP);
                                                                                                                       // 에러 메세지 설정
    sprintf(response_header, "HTTP/1.0 200 OK\r\nServer: 2023 web server\r\nContent-length: %ld\r\nContent-Type: text/html\r\n\r\n", strlen(error_message)+1); // 헤더 메세지 설정

    write(client_fd, response_header, strlen(response_header)+1); // 응답 메세지 헤더 write
    write(client_fd, error_message, strlen(error_message)+1);     // 에러 응답 메세지 write
    fclose(file);
    return 0;
}

void printServerHistory() {

    printf("========= Connection History ===================================\n");    
    printf("Number of request(s) : %d\n", request);
    if(request > 0) {
        printf("No.\tIP\t\tPID\tPORT\tTIME\n");
        for(int i = 0; i < 10; i++) {
            printf("%d\t%s\t%d\t%d\t\n", request-(9-i), IP[i], PID[i], Port[i]);

            struct tm tm = *localtime(&TIME[i]);
            char buffer[80]; 
            //strftime(buffer, 80, "%a %b %d %H:%M:%S %Y\n\n", &tm);
        }
    }
    No = 0;
}

void saveConnectHistory(struct sockaddr_in client_addr) {

    if(No < 10) {
        Port[No] = ntohs(client_addr.sin_port);
        strcpy(IP[No], inet_ntoa(client_addr.sin_addr));
        TIME[No] = time(NULL);
        PID[No] = getpid();
        ++No;
    }

    else {
        for(int i = 0; i < 10; i++) {
            Port[i] = Port[i+1];
            strcpy(IP[i], IP[i+1]);
            TIME[i] = TIME[i+1];
            PID[i] = PID[i+1];
        }
        Port[9] = ntohs(client_addr.sin_port);
        strcpy(IP[9], inet_ntoa(client_addr.sin_addr));
        TIME[9] = time(NULL);
        PID[9] = getpid();
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// sendResponse                                                                      //
// --------------------------------------------------------------------------------- //
// Input: char* url -> The URL address requested by the client                       //
//        char* response_header -> Response message header from the server           //
//        int isNotStart -> Distinguishing between initial input                     //
//        int client_fd -> File descriptor of the client                             //
// output:                                                                           //
// purpose: Classify whether the address entered as a URL is a directory, plain file // 
//          , or image, then perform file read to create a response message and send //
//          it with a header.                                                        //
///////////////////////////////////////////////////////////////////////////////////////
void sendResponse(char* url, char* response_header, int isNotStart, int client_fd) {

    FILE *file; //url로 열 파일
    struct dirent *dir; //파일 정보를 담을 구조체
    int count = 0, isNotFound = 0; //파일의 길이, 파일의 존재 여부
    char file_extension[10]; // 파일 확장자를 저장할 배열
    char content_type[30];   // MIME TYPE을 저장할 배열
    char *response_message = NULL; //서버의 응답 메세지

    if(isNotStart == 0) //root path로 접근할 때(처음 접속)
        isNotFound = writeHTMLFile(url); //존재하는 디렉토리인지 확인
    
    if (isNotFound == 1) { //존재하지 않는 디렉토리라면

        char error_message[MAX_LENGTH]; //서버의 에러 응답 메세지
        sprintf(error_message, "<h1>Not Found</h1><br>"
                                  "The request URL %s was not found on this server<br>"
                                  "HTTP 404 - Not Page Found", url); //에러 메세지 설정
        sprintf(response_header, "HTTP/1.0 200 OK\r\nServer: 2023 web server\r\nContent-length: %ld\r\nContent-Type: text/html\r\n\r\n", strlen(error_message)+1); //헤더 메세지 설정

        write(client_fd, response_header, strlen(response_header)); //응답 메세지 헤더 write
        write(client_fd, error_message, strlen(error_message)); //에러 응답 메세지 write
        return; //함수 종료
    }

    //이미지 파일인 경우
    if ((fnmatch("*.jpg", url, FNM_CASEFOLD) == 0) || (fnmatch("*.png", url, FNM_CASEFOLD) == 0) || (fnmatch("*.jpeg", url, FNM_CASEFOLD) == 0)) 
        strcpy(content_type, "image/*"); //content-type을 image/*로 설정

    else { //디렉토리 또는 일반 파일인 경우

        char *dot = strrchr(url, '.'); //확장자 찾기
        if (dot && dot != url) //확장자가 존재하는 경우
            strcpy(file_extension, dot + 1); //확장자명을 file_extension에 저장

        char dirPath[MAX_LENGTH] = {'\0', }; //파일 또는 디렉토리의 절대 경로
        getAbsolutePath(url, dirPath); //파일 또는 디렉토리의 절대경로 받아오기

        if((opendir(dirPath) != NULL) || (isNotStart == 0)) { //디렉토리인 경우 또는 root path인 경우
            strcpy(content_type, "text/html"); //content-type을 text/html로 설정
            writeHTMLFile(url); //html 파일에 ls 결과 입력
        }
        else
            strcpy(content_type, "text/plain"); //content-type을 text/plain으로 설정
    }

    if (strcmp(content_type, "text/html") == 0) //디렉토리 주소를 입력받은 경우
        file = fopen("dir_file_list.html", "r"); //html 파일(ls로 결과가 table로 출력) open
    else if(strcmp(content_type, "text/plain") == 0)
        file = fopen(url, "r");
    else
        file = fopen(url, "rb"); //이미지를 읽어오는 경우

    fseek(file, 0, SEEK_END); //파일의 끝부분으로 이동
    count = ftell(file); //파일의 크기를 count에 저장
    fseek(file, 0, SEEK_SET); //파일의 가장 앞으로 이동
    rewind(file);

    response_message = (char *)malloc((sizeof(char)) * (count+1)); //count+1의 크기만큼 response_message 크기 지정

    if(strcmp(content_type, "image/*") == 0) //이미지 파일인 경우
        fread(response_message, 1, count+1, file); //이미지 바이너리 파일 read 내용을 응답 메세지에 저장
    else //일반 파일 및 디렉토리(html 문서)인 경우
        fread(response_message, sizeof(char), count+1, file); //파일 read 내용을 응답 메세지에 저장

    fclose(file); //file close

    sprintf(response_header, "HTTP/1.0 200 OK\r\nServer: 2023 web server\r\nContent-length: %d\r\nContent-Type: %s\r\n\r\n", count+1, content_type); //서버 응답 메세지 헤더 설정
    write(client_fd, response_header, strlen(response_header)); //서버 응답 메세지 헤더 출력
    write(client_fd, response_message, count+1); //서버 응답 메세지 출력
}

///////////////////////////////////////////////////////////////////////////////////////
// writeHTMLFile                                                                     //
// --------------------------------------------------------------------------------- //
// Input: char* url -> The URL address requested by the client                       //
// output:                                                                           //
// purpose: If the requested file is a directory, input the ls results for the files //
//          in the directory into an HTML document.                                  //
///////////////////////////////////////////////////////////////////////////////////////
int writeHTMLFile(char* url) {
    
    struct dirent *dir; //dirent 구조체
    char Apath[MAX_LENGTH] = {'\0', }; //working directory의 절대 경로
    char title[MAX_LENGTH] = "dir_file_list.html"; // html 문서
    
    FILE *file = fopen("dir_file_list.html", "w"); //html 파일 생성 및 오픈

    fprintf(file, "<!DOCTYPE html>\n<html>\n<head>\n"); //title 생성
    getAbsolutePath(title, Apath); //html 문서의 절대 경로 받아오기
    fprintf(file, "<title>%s</title>\n</head>\n<body>\n", Apath); //절대경로로 title 설정

    if(url[1] == '\0') { //root path인 경우

        fprintf(file, "<h1>Welcome to System Programming Http</h1>\n<br>\n"); //header 작성
        char currentPath[10] = "."; //현재 경로
        listDirFiles(0, 1, 0, 0, 0, currentPath, file); //현재 디렉토리 하위 파일 출력
    }

    else { //root path가 아닌 경우
        char dirPath[MAX_LENGTH] = {'\0', }; //절대경로를 받아올 배열

        fprintf(file, "<h1>System Programming Http</h1>\n<br>\n"); //header 작성
        
        getAbsolutePath(url, dirPath); //dirPath에 url의 절대경로를 받아오기
        if(opendir(dirPath) == NULL) //url이 디렉토리가 아니라면
            return 1; //함수 종료

        listDirFiles(1, 1, 0, 0, 0, url, file); //url이 디렉토리라면 listDirFiles() 실행
    }

    fclose(file); //file close
    return 0; //함수 종료
}

///////////////////////////////////////////////////////////////////////////////////////
// listDirFiles                                                                      //
// --------------------------------------------------------------------------------- //
// Input: int a_hidden -> option -a                                                  //
//        int l_format -> option -l                                                  //
//        char* filename -> file name that provided                                  //
// output:                                                                           //
// purpose: Prints sub-files in the directory specified by the filename argument     //
//          based on the options                                                     //
///////////////////////////////////////////////////////////////////////////////////////
void listDirFiles(int a_hidden, int l_format, int S_size, int r_reverse, int h_readable, char* filename, FILE *file) {

    DIR *dirp; //dir 포인터
    struct dirent *dir; //dirent 구조체
    struct stat st, fileStat; //파일 속성정보 저장할 구조체
    int fileNum = 0, realfileNum = 0; //파일의 개수
    char timeBuf[80]; //시간 정보
    int total = 0; //총 블락 수
    char accessPath[MAX_LENGTH], accessFilename[MAX_LENGTH], dirPath[MAX_LENGTH] = {'\0', }; //여러 경로들
    int* isHidden = (int*)calloc(fileNum, sizeof(int)); //히든파일 여부 판별

    getAbsolutePath(filename, dirPath);
    dirp = opendir(dirPath); //절대경로로 opendir

    char temp[MAX_LENGTH];
    getcwd(temp, MAX_LENGTH);
    if(strcmp(dirPath, temp) == 0)
        a_hidden = 0;

    while((dir = readdir(dirp)) != NULL) { //디렉토리 하위 파일들을 읽어들임

        joinPathAndFileName(accessFilename, dirPath, dir->d_name);
        lstat(accessFilename, &st); //파일의 절대 경로로 lstat() 호출

        if(a_hidden == 1 || dir->d_name[0] != '.') {
            total += st.st_blocks; //옵션(-a)에 따라 total 계산           
            ++realfileNum; //총 파일개수 count
        }
        ++fileNum; //총 파일개수 count
    }
    
    rewinddir(dirp); //dirp 처음으로 초기화

    char **fileList = (char**)malloc(sizeof(char*) * (fileNum+1)); //동적 할당
    for(int i = 0; i < fileNum; i++) { //파일 개수만큼 반복
        
        fileList[i] = (char*)malloc(sizeof(char) * 300); //동적 할당
        dir = readdir(dirp); //디렉토리 내 파일 읽기

        if(strcmp(dir->d_name, "dir_file_list.html") != 0) //html 실행 파일이 아니라면
            strcpy(fileList[i], dir->d_name); //fileList에 파일명 저장
        else { //html 실행 파일이라면
            i--; //인덱스 감소
            realfileNum--; //실제 파일 수 감소
            fileNum--; //파일 개수 감소
        }
    }

    sortByNameInAscii(fileList, fileNum, 0, r_reverse); //아스키 코드순으로 정렬
    fprintf(file, "<b>Directory path: %s</b><br>\n", dirPath); //파일 경로 출력
    
    if(l_format == 1) //옵션 -l이 포함된 경우
        fprintf(file, "<b>total : %d</b>\n", (int)(total/2));

    if(realfileNum == 0) { //만약 html 실행 파일과 히든 파일을 제외한 파일이 없다면
        fprintf(file, "<br><br>\n");
        return;
    }

    fprintf(file, "<table border=\"1\">\n<tr>\n<th>Name</th>\n"); // 테이블 생성
    if (l_format == 1) {
        fprintf(file, "<th>Permissions</th>\n");        // 권한 열 생성
        fprintf(file, "<th>Link</th>\n");               // 링크 열 생성
        fprintf(file, "<th>Owner</th>\n");              // 소유자 열 생성
        fprintf(file, "<th>Group</th>\n");              // 소유 그룹 열 생성
        fprintf(file, "<th>Size</th>\n");               // 사이즈 열 생성
        fprintf(file, "<th>Last Modified</th>\n</tr>"); // 마지막 수정날짜 열 생성
    }
    else
        fprintf(file, "</tr>"); //header raw 닫기

    for (int i = 0; i < fileNum; i++) { // 파일 개수만큼 반복

        if ((a_hidden == 0) && fileList[i][0] == '.') // 옵션 -a 여부에 따라 파일속성 출력
            continue;

        joinPathAndFileName(accessPath, dirPath, fileList[i]); // 파일과 경로를 이어붙이기
        lstat(accessPath, &fileStat);                           // 파일 속성정보 불러옴

        char color[20] = {'\0',}; // 파일의 색상 저장할 배열
        findColor(accessPath, color); // 색 찾기

        fprintf(file, "<tr style=\"%s\">\n", color);
        fprintf(file, "<td><a href=%s>%s</a></td>", accessPath, fileList[i]); // 파일 이름 및 링크 출력

        if (l_format == 1)                                      // 옵션 -l이 포함된 경우
            printAttributes(fileStat, file, h_readable, color); // 속성 정보 출력

        else
            fprintf(file, "</tr>"); //raw 닫기
    }
    fprintf(file, "</table>\n<br>\n"); //table 닫기
}

///////////////////////////////////////////////////////////////////////////////////////
// getAbsolutePath                                                                   //
// --------------------------------------------------------------------------------- //
// Input: char* inputPath -> Path input                                              //
//        char* absolutePath -> path that made asolute                               //
// output:                                                                           //
// purpose: Finds the absolute path of the inputted directory or file.               //
///////////////////////////////////////////////////////////////////////////////////////
void getAbsolutePath(char *inputPath, char *absolutePath) {

    getcwd(absolutePath, MAX_LENGTH); //현재 경로 받아옴

    if(inputPath[0] != '/') //입력이 절대경로가 아닌 파일이고, /로 시작하지 않을 때 ex) A/*
        strcat(absolutePath, "/"); // /을 제일 뒤에 붙여줌
    
    if(strstr(inputPath, absolutePath) != NULL) //입력받은 경로가 현재 경로를 포함할 때 ex)/home/Assignment/A/*
        strcpy(absolutePath, inputPath); //입력받은 경로로 절대경로 덮어쓰기
    else
        strcat(absolutePath, inputPath); //현재 경로에 입력받은 경로 이어붙이기

    int i, j, len = strlen(absolutePath);
    for (i = j = 0; i < len; i++) {
        if (i > 0 && absolutePath[i] == '/' && absolutePath[i-1] == '/') {
            // do nothing
        } else {
            absolutePath[j++] = absolutePath[i];
        }
    }
    absolutePath[j] = '\0';
}

///////////////////////////////////////////////////////////////////////////////////////
// joinPathAndFileName                                                               //
// --------------------------------------------------------------------------------- //
// Input: char* inputPath -> new array that concatenated                             //
//        char* Apath -> absolute path as input                                      //
//        char* fileName -> file name that appended                                  //
// output:                                                                           //
// purpose: Receives an absolute path as input, along with a file name to be         //
//          appended, and generates a new array representing the concatenated path.  //
///////////////////////////////////////////////////////////////////////////////////////
void joinPathAndFileName(char* path, char* Apath, char* fileName) {

    strcpy(path, Apath); //입력받은 경로 불러오기
    strcat(path, "/"); // /를 붙이고
    strcat(path, fileName); //읽어온 파일명 붙이기
}

///////////////////////////////////////////////////////////////////////////////////////
// printAttributes                                                                   //
// --------------------------------------------------------------------------------- //
// Input: struct stat fileStat -> Save information about a file (such as file size   //
//                                , owner, permissions, etc.)                        //
// output:                                                                           //
// purpose: Prints the attributes of the file using the information from the given   //
//          name of struct stat object                                               //
///////////////////////////////////////////////////////////////////////////////////////
void printAttributes(struct stat fileStat, FILE* file, int h_readable, char *color) {
    
    char timeBuf[80]; //시간 정보 받아올 변수

    printType(fileStat, file); // 파일 유형
    printPermissions(fileStat.st_mode, file); // 허가권
    fprintf(file, "<td>%ld</td>", fileStat.st_nlink); // 링크 수
    fprintf(file, "<td>%s</td><td>%s</td>", getpwuid(fileStat.st_uid)->pw_name, getgrgid(fileStat.st_gid)->gr_name); // 파일 소유자 및 파일 소유 그룹

    if(h_readable == 1) { //만약 h 속성이 존재한다면
        
        double size = (double)fileStat.st_size; //파일의 사이즈를 받아오기
        int remainder = 0; //1024로 나눈 나머지를 계속 저장할 변수
        char sizeUnit[3] = {'K', 'M', 'G'}; //K, M, G 단위
        int unit = 0, unitIndex = -1; //유닛 인덱스로 단위 붙이기

        while (size >= 1024) { //1024보다 클 경우
            size /= 1024; //1024로 나눈 몫
            remainder %= 1024; //1024로 나눈 나머지
            unit = 1; //단위를 붙여야 함
            unitIndex++; //유닛 인덱스 증가
        }

        if(unit == 1) //단위를 붙여야 한다면
            fprintf(file, "<td>%.1f%c</td>", size, sizeUnit[unitIndex]); //소수점 아래 한 자리까지 출력
        else //안붙여도 될 정도로 작다면
            fprintf(file, "<td>%.0f</td>", size); //소수점 버리고 출력
    }
    else
        fprintf(file, "<td>%ld</td>", fileStat.st_size); // 파일 사이즈

    strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M", localtime(&fileStat.st_mtime)); // 수정된 날짜 및 시간 불러오기
    fprintf(file, "<td>%s</td>", timeBuf); // 수정된 날짜 및 시간 출력

    fprintf(file, "</tr>\n");
}

///////////////////////////////////////////////////////////////////////////////////////
// sortByNameInAscii                                                                 //
// --------------------------------------------------------------------------------- //
// Input: char **fileList -> An array containing file names.                         //
//        int fileNum -> size of fileList                                            //
// output:                                                                           //
// purpose: Sort the filenames in alphabetical order (ignoring case) without the dot //
///////////////////////////////////////////////////////////////////////////////////////
void sortByNameInAscii(char **fileList, int fileNum, int start, int r_reverse)
{
    int* isHidden = (int*)calloc(fileNum, sizeof(int)); //hidden file인지 판별 후 저장
    
    for (int i = start; i < fileNum; i++) { //파일리스트 반복문 실행
         if ((fileList[i][0] == '.') && (strcmp(fileList[i], ".") != 0) && (strcmp(fileList[i], "..") != 0)) { //hidden file인 경우
            isHidden[i] = 1; //파일명 가장 앞의 . 제거
            for (int k = 0; k < strlen(fileList[i]); k++) //파일 글자수 반복
                fileList[i][k] = fileList[i][k + 1]; //앞으로 한 칸씩 땡기기
        }
    }

    for (int i = start; i < (fileNum - 1); i++) { // 대소문자 구분 없는 알파벳 순으로 정렬
        for (int j = i + 1; j < fileNum; j++) { //bubble sort
            if (((compareStringUpper(fileList[i], fileList[j]) == 1) && (r_reverse == 0)) || ((compareStringUpper(fileList[i], fileList[j]) == 0) && (r_reverse == 1))) {
            //만약 첫 문자열이 둘째 문자열보다 작다면
                char *temp = fileList[i]; // 문자열 위치 바꾸기
                fileList[i] = fileList[j];
                fileList[j] = temp;

                int temp2 = isHidden[i]; //히든파일인지 저장한 배열도 위치 바꾸기
                isHidden[i] = isHidden[j];
                isHidden[j] = temp2;
            }
        }
    }

    for (int i = start; i < fileNum; i++) { //리스트 반복문 돌리기
        if(isHidden[i] == 1) { //hidden file인 경우
            for(int k = strlen(fileList[i]); k >= 0; k--) //파일 길이만큼 반복
                fileList[i][k+1] = fileList[i][k]; //뒤로 한 칸씩 보내기
            fileList[i][0] = '.'; //파일명 가장 앞에 . 다시 추가
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// compareStringUpper                                                                //
// --------------------------------------------------------------------------------- //
// Input: char* fileName1 -> The first string to compare                             //
//        char* fileName2 -> The second string to compare                            //
// output: 0 -> No need to swap positions, 1 -> Need to swap positions               //
// purpose: Comparing two strings based on uppercase letters to determine which one  //
//          is greater.                                                              //
///////////////////////////////////////////////////////////////////////////////////////
int compareStringUpper(char* fileName1, char* fileName2) {
    
    char* str1 = (char*)calloc(strlen(fileName1)+1, sizeof(char)); //비교할 첫 번째 문자열
    char* str2 = (char*)calloc(strlen(fileName2)+1, sizeof(char)); //비교할 두 번째 문자열

    for(int i = 0; i < strlen(fileName1); i++) //첫 번째 문자열을 돌면서
        str1[i] = toupper(fileName1[i]); //모두 대문자로 전환

    for(int i = 0; i < strlen(fileName2); i++) //두 번째 문자열을 돌면서
        str2[i] = toupper(fileName2[i]); //모두 대문자로 전환

    if((strcmp(str1, ".") == 0 || strcmp(str1, "..") == 0) && (strcmp(str2, ".") != 0)) //위치를 바꿀 필요가 없는 경우
        return 0; //0 반환

    else if((strcmp(str2, ".") == 0 || strcmp(str2, "..") == 0) && (strcmp(str1, ".") != 0)) //위치를 바꿀 필요가 있는 경우
        return 1; //1 반환

    else if(strcmp(str1, str2) > 0) //위치를 바꿀 필요가 있는 경우
        return 1; //1 반환
    
    return 0; //위치를 바꿀 필요가 없는 경우 0 반환
}

///////////////////////////////////////////////////////////////////////////////////////
// printPermissions                                                                  //
// --------------------------------------------------------------------------------- //
// Input: mode_t mode -> represents the permission information of a file.            //
// output:                                                                           //
// purpose: Printing file permissions for user, group, and others.                   //
///////////////////////////////////////////////////////////////////////////////////////
void printPermissions(mode_t mode, FILE *file) {
    fprintf(file, "%c", (mode & S_IRUSR) ? 'r' : '-'); //user-read
    fprintf(file, "%c", (mode & S_IWUSR) ? 'w' : '-'); //user-write
    fprintf(file, "%c", (mode & S_IXUSR) ? 'x' : '-'); //user-execute
    fprintf(file, "%c", (mode & S_IRGRP) ? 'r' : '-'); //group-read
    fprintf(file, "%c", (mode & S_IWGRP) ? 'w' : '-'); //group-write
    fprintf(file, "%c", (mode & S_IXGRP) ? 'x' : '-'); //group-execute
    fprintf(file, "%c", (mode & S_IROTH) ? 'r' : '-'); //other-read
    fprintf(file, "%c", (mode & S_IWOTH) ? 'w' : '-'); //other-write
    fprintf(file, "%c", (mode & S_IXOTH) ? 'x' : '-'); //other-execute

    fprintf(file, "</td>");
}

///////////////////////////////////////////////////////////////////////////////////////
// printType                                                                         //
// --------------------------------------------------------------------------------- //
// Input: struct stat fileStat -> Save information about a file (such as file size   //
//                                , owner, permissions, etc.)                        //
// output:                                                                           //
// purpose: Printing file type(regular file, directory, symbolic link, etc.)         //
///////////////////////////////////////////////////////////////////////////////////////
void printType(struct stat fileStat, FILE *file) {

    fprintf(file, "<td>");

    switch (fileStat.st_mode & S_IFMT) {
    case S_IFREG: //regular file
        fprintf(file, "-");
        break;
    case S_IFDIR: //directory
        fprintf(file, "d");
        break;
    case S_IFLNK: //symbolic link
        fprintf(file, "l");
        break;
    case S_IFSOCK: //socket
        fprintf(file, "s");
        break;
    case S_IFIFO: //FIFO(named pipe)
        fprintf(file, "p");
        break;
    case S_IFCHR: //character device
        fprintf(file, "c");
        break;
    case S_IFBLK: //block device
        fprintf(file, "b");
        break;
    default:
        fprintf(file, "?"); //unknown
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// findColor                                                                         //
// --------------------------------------------------------------------------------- //
// Input: char* fileName -> File name to determine the file type                     //
//        char* color -> Array to store colors based on the file type                //
// output:                                                                           //
// purpose: Detects the file type based on the inputted file name and stores the     //
//          corresponding color in an array.                                         //
///////////////////////////////////////////////////////////////////////////////////////
void findColor(char* fileName, char* color) {

    struct stat fileStat; //파일 속성정보
    lstat(fileName, &fileStat); //파일 경로로 속성 정보 받아오기

    if ((fileStat.st_mode & S_IFMT) == S_IFDIR)      // 디렉토리일 경우
        strcpy(color, "color: Blue");                // 파랑 출력
    else if ((fileStat.st_mode & S_IFMT) == S_IFLNK) // 심볼릭 링크일 경우
        strcpy(color, "color: Green");               // 초록 출력
    else                                             // 그 외 파일들의 경우
        strcpy(color, "color: Red");                 // 빨강 출력
}