/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 12/08/2013
 * 
 * Um código simples (não é o código ideal, mas é o suficiente para o
 * EP) de um servidor de eco a ser usado como base para o EP1. Ele
 * recebe uma linha de um cliente e devolve a mesma linha. Teste ele
 * assim depois de compilar:
 * 
 * ./servidor 8000
 * 
 * Com este comando o servidor ficará escutando por conexões na porta
 * 8000 TCP (Se você quiser fazer o servidor escutar em uma porta
 * menor que 1024 você precisa ser root).
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequências de caracteres seguidas de ENTER. Você verá que
 * o telnet exige a mesma linha em seguida. Esta repetição da linha é
 * enviada pelo servidor. O servidor também exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 * 
 * Obs.: Você pode conectar no servidor remotamente também. Basta saber o
 * endereço IP remoto da máquina onde o servidor está rodando e não
 * pode haver nenhum firewall no meio do caminho bloqueando conexões na
 * porta escolhida.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096
#define BAD_REQUEST(connfd) { bad_request(connfd); return; }

static int listenfd;

void sendFileToSocket(int sock, char* filename);

void intHandler(int dummy) {
    close(listenfd);
    exit(0);
}

void fill_header(char* out, const char* status) {
	// Following http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html
	char datebuffer[80];
	time_t rawtime;

	time(&rawtime);
	struct tm* timeinfo = localtime(&rawtime);
	strftime(datebuffer,80,"%Y-%m-%d-%H-%M-%S",timeinfo);

	int line_end = sprintf(out, "HTTP/1.1 %s\r\n", status);
	line_end += sprintf(out + line_end, "Date: %s\r\n", datebuffer);
	line_end += sprintf(out + line_end, "Connection: close\r\n");
	line_end += sprintf(out + line_end, "Server: Servidor Do Macaco v0.1-54-c84bs93-dirty\r\n");
}

void bad_request(int connfd) {
	char out[5*MAXLINE];
	fill_header(out, "400 Bad Request");
	strcat(out, "Content-Type: text/html; charset=UTF-8\r\n");
	strcat(out, "\r\n");
	strcat(out, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n\
<html><head>\n\
<title>400 Bad Request</title>\n\
</head><body>\n\
<h1>Bad Request</h1>\n\
<p>Your browser sent a request that this server could not understand.<br />\n\
</p>\n\
<hr>\n\
</body></html>");
    write(connfd, out, strlen(out));
}

void fourohfour(int connfd) {
	char out[5*MAXLINE];
	fill_header(out, "404 Not Found");
	strcat(out, "Content-Type: text/html; charset=UTF-8\r\n");
	strcat(out, "\r\n");
	strcat(out, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n\
<html><head>\n\
<title>404 Not Found</title>\n\
</head><body>\n\
<h1>Not Found</h1>\n\
<p>The requested URL was not found on this server.</p>\n\
<hr>\n\
</body></html>");
    write(connfd, out, strlen(out));
}

void get_request(int connfd, char* recvline) {
    if(recvline[0] != '/') BAD_REQUEST(connfd);
    char* space = strchr(recvline, ' ');
    
    if(space != NULL) { // Bad request... mas só ignorar.
        space[0] = '\0';
    }
    
    int recvline_len = strlen(recvline) - 1;
    while(isspace(recvline[recvline_len]))
        recvline[recvline_len--] = '\0';
    
    static const char* base_path = ".";
    char filepath_buffer[MAXLINE + 1];
    int offset = 0;
       
    offset += snprintf(filepath_buffer + offset, MAXLINE - offset, "%s", base_path);
    offset += snprintf(filepath_buffer + offset, MAXLINE - offset, "%s", recvline);
    if(filepath_buffer[offset-1] == '/')
        offset += snprintf(filepath_buffer + offset, MAXLINE - offset, "%s", "index.html");
    
    printf("'%s'\n", filepath_buffer);
     
    sendFileToSocket(connfd, filepath_buffer);
}

void post_request(int connfd, char* recvline) {
	char* empty_line = recvline;
	do {
		empty_line = strchr(empty_line + 1, '\n');
		if(!empty_line) {
			bad_request(connfd);
			return;
		}
	} while(empty_line[1] != '\r');

	// Achamos a linha 
	char *argumentos = empty_line + 3; // escovando string
	
	for(char* troca_e_por_lines = argumentos; (troca_e_por_lines = strchr(troca_e_por_lines, '&')) != NULL; troca_e_por_lines[0] = '\n');

	char* argumento = argumentos;
	while(argumento != NULL) {
		char* next = strchr(argumento, '\n');
		if(next) next[0] = '\0';

		char* nome = argumento;
		char* valor = strchr(argumento, '=');
		if(!valor) BAD_REQUEST(connfd);
		valor[0] = '\0';
		valor++;

		printf("Peguei um argumento com nome '%s' e valor '%s'\n", nome, valor);

		argumento = (next != NULL) ? (next + 1) : NULL;
	}

    get_request(connfd, recvline);
}

void options_request(int connfd, char* recvline) {
	char out[5*MAXLINE];
	fill_header(out, "200 OK");
	strcat(out, "Allow: GET,POST,OPTIONS\r\n");
	strcat(out, "\r\n");
    write(connfd, out, strlen(out));
}

void handle_client(int connfd, char* recvline) {
    while(isspace(recvline[0])) ++recvline;
    
    puts(recvline);
    
    char* space = strchr(recvline, ' ');
    if(space == NULL) BAD_REQUEST(connfd);
    space[0] = '\0';
    if(strcmp("GET", recvline) == 0)
        get_request(connfd, space+1);
    else if(strcmp("POST", recvline) == 0)
        post_request(connfd, space+1);
    else if(strcmp("OPTIONS", recvline) == 0)
        options_request(connfd, space+1);
    else
        bad_request(connfd);
}

int main (int argc, char **argv) {
   /* Os sockets. Um que será o socket que vai escutar pelas conexões
    * e o outro que vai ser o socket especÃ­fico de cada conexão */
	int connfd;
   /* Informações sobre o socket (endereço e porta) ficam nesta struct */
	struct sockaddr_in servaddr;
   /* Retorno da função fork para saber quem é o processo filho e quem
    * é o processo pai */
   pid_t childpid;
   /* Armazena linhas recebidas do cliente */
	char	recvline[MAXLINE + 1];
   /* Armazena o tamanho da string lida do cliente */
   ssize_t  n;

    signal(SIGINT, intHandler);
   
	if (argc != 2) {
      fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
      fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
		exit(1);
	}

   /* Criação de um socket. Eh como se fosse um descritor de arquivo. Eh
    * possivel fazer operacoes como read, write e close. Neste
    * caso o socket criado eh um socket IPv4 (por causa do AF_INET),
    * que vai usar TCP (por causa do SOCK_STREAM), já que o HTTP
    * funciona sobre TCP, e será usado para uma aplicação convencional sobre
    * a Internet (por causa do número 0) */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket :(\n");
		exit(2);
	}

   /* Agora é necessário informar os endereços associados a este
    * socket. É necessário informar o endereço / interface e a porta,
    * pois mais adiante o socket ficará esperando conexões nesta porta
    * e neste(s) endereços. Para isso é necessário preencher a struct
    * servaddr. É necessário colocar lá o tipo de socket (No nosso
    * caso AF_INET porque é IPv4), em qual endereço / interface serão
    * esperadas conexões (Neste caso em qualquer uma -- INADDR_ANY) e
    * qual a porta. Neste caso será a porta que foi passada como
    * argumento no shell (atoi(argv[1]))
    */
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(atoi(argv[1]));
	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		perror("bind :(\n");
		exit(3);
	}

   /* Como este código é o código de um servidor, o socket será um
    * socket passivo. Para isto é necessário chamar a função listen
    * que define que este é um socket de servidor que ficará esperando
    * por conexões nos endereços definidos na função bind. */
	if (listen(listenfd, LISTENQ) == -1) {
		perror("listen :(\n");
		exit(4);
	}

   printf("[Servidor no ar. Aguardando conexoes na porta %s]\n",argv[1]);
   printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
   /* O servidor no final das contas é um loop infinito de espera por
    * conexões e processamento de cada uma individualmente */
	for (;;) {
      /* O socket inicial que foi criado é o socket que vai aguardar
       * pela conexão na porta especificada. Mas pode ser que existam
       * diversos clientes conectando no servidor. Por isso deve-se
       * utilizar a função accept. Esta função vai retirar uma conexão
       * da fila de conexões que foram aceitas no socket listenfd e
       * vai criar um socket especÃ­fico para esta conexão. O descritor
       * deste novo socket é o retorno da função accept. */
		if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
			perror("accept :(\n");
			exit(5);
		}
      
      /* Agora o servidor precisa tratar este cliente de forma
       * separada. Para isto é criado um processo filho usando a
       * função fork. O processo vai ser uma cópia deste. Depois da
       * função fork, os dois processos (pai e filho) estarão no mesmo
       * ponto do código, mas cada um terá um PID diferente. Assim é
       * possÃ­vel diferenciar o que cada processo terá que fazer. O
       * filho tem que processar a requisição do cliente. O pai tem
       * que voltar no loop para continuar aceitando novas conexões */
      /* Se o retorno da função fork for zero, é porque está no
       * processo filho. */
      if ( (childpid = fork()) == 0) {
         /**** PROCESSO FILHO ****/
         printf("[Uma conexao aberta]\n");
         /* Já que está no processo filho, não precisa mais do socket
          * listenfd. Só o processo pai precisa deste socket. */
         close(listenfd);
         
         /* Agora pode ler do socket e escrever no socket. Isto tem
          * que ser feito em sincronia com o cliente. Não faz sentido
          * ler sem ter o que ler. Ou seja, neste caso está sendo
          * considerado que o cliente vai enviar algo para o servidor.
          * O servidor vai processar o que tiver sido enviado e vai
          * enviar uma resposta para o cliente (Que precisará estar
          * esperando por esta resposta) 
          */

         /* ========================================================= */
         /* ========================================================= */
         /*                         EP1 INÍCIO                        */
         /* ========================================================= */
         /* ========================================================= */
         /* TODO: É esta parte do código que terá que ser modificada
          * para que este servidor consiga interpretar comandos HTTP */
         while ((n=read(connfd, recvline, MAXLINE)) > 0) {
            recvline[n]=0;
            printf("[Cliente conectado no processo filho %d enviou:] ",getpid());
            handle_client(connfd, recvline);
            close(connfd);
         }
         /* ========================================================= */
         /* ========================================================= */
         /*                         EP1 FIM                           */
         /* ========================================================= */
         /* ========================================================= */

         /* Após ter feito toda a troca de informação com o cliente,
          * pode finalizar o processo filho */
         printf("[Uma conexao fechada]\n");
         exit(0);
      }
      /**** PROCESSO PAI ****/
      /* Se for o pai, a única coisa a ser feita é fechar o socket
       * connfd (ele é o socket do cliente específico que será tratado
       * pelo processo filho) */
		close(connfd);
	}
	exit(0);
}

const char* content_type_from(const char* filename) {
    static const char* BINARY = "application/octet-stream";
    static const char* HTML = "text/html";
    static const char* PNG = "image/png";
    static const char* JPEG = "image/jpeg";
    static const char* PLAIN = "text/plain";

    char* dot = strrchr(filename, '.');
    if(!dot) return BINARY;
    if(strcmp(dot + 1, "html") == 0)
        return HTML;
    if(strcmp(dot + 1, "png") == 0)
        return PNG;
    if(strcmp(dot + 1, "jpeg") == 0 || strcmp(dot + 1, "jpg") == 0)
        return JPEG;
    if(strcmp(dot + 1, "txt") == 0)
        return PLAIN;
    return BINARY;
}

// Taken from StackOverflow
void sendFileToSocket(int sock, char* filename) { 
    char buf[1024]; 
    FILE *file = fopen(filename, "rb"); 
    if (!file)
    {   // can't open file
        fourohfour(sock);
        return;
    }
    
    char headers_buffer[MAXLINE + 1];
    snprintf(headers_buffer, MAXLINE, "HTTP/1.1 200 OK\nContent-Type: %s; charset=UTF-8\nConnection: close\n\n", content_type_from(filename));
    write(sock, headers_buffer, strlen(headers_buffer));
    while (!feof(file)) 
    { 
        int rval = fread(buf, 1, sizeof(buf), file); 
        int off = 0;
        do
        {
            int sent = write(sock, &buf[off], rval - off);
            if (sent < 1)
            {
                // if the socket is non-blocking, then check
                // the socket error for WSAEWOULDBLOCK/EAGAIN
                // (depending on platform) and if true then
                // use select() to wait for a small period of
                // time to see if the socket becomes writable
                // again before failing the transfer...

                printf("Can't write to socket");
                fclose(file);
                return;
            }

            off += sent;
        }
        while (off < rval);
    } 

    fclose(file);
}
