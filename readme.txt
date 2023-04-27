/*makefile을 통해 실행 파일 컴파일 방법*/
(Linux 환경에서) 이 폴더에서 터미널을 연 후(혹은 절대경로를 알아서 찾아서 하든) 터미널에 make만을 입력한다.
결과물로 http_server.o, myserver가 만들어질텐데, myserver로 실행하면 된다.

/* 서버 테스트 방법 */
(Linux 환경에서) ./myserver {port_num}을 터미널에 입력한다(실행 경로 맞춰서. ./대신 절대경로도 무방)
구글 크롬(추천), 또는 아무 브라우저에서 localhost:{port_num}/파일경로를 입력한다
현재 클라이언트에서 브라우저에 입력 가능한 주소는 다음과 같다

localhost:{port_num}/audio/onestop.mp3
localhost:{port_num}/img/blink.gif
localhost:{port_num}/img/lena.jpg
localhost:{port_num}/document/test.pdf
localhost:{port_num}/index.html
