#include <stdio.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>

struct sockaddr_in local, remote;
char response[1000];

struct header {
        char * n;
        char * v;
} h[100];

char converter(char ch){
        char a = (char)ch;
        if (a>=0&&a<=25) return a+65;
        if (a>=26&&a<=51) return a+71;
        if (a>=52&&a<=61) return a-4;
        if (a==62) return 43;
        if (a==63) return 47;
}

int main(){
        char hbuffer[10000], rawbuff[1000], in64buff[1500];
        char *reqline, *method, *url, *ver, *filename, *auth64;
        FILE *fin, *fpass;
        int c, i, j, k, t, s, s2, len;
        int yes = 1, match = 1, authhead = 0;
        int cnt = 0, cnt64 = 0, ibuff = 0;
        char prev;
        if (( s = socket(AF_INET, SOCK_STREAM, 0 )) == -1){
                printf("errno = %d\n",errno);
                perror("Socket Fallita");
                return -1;
        }
        local.sin_family = AF_INET;
        local.sin_port = htons(16393);
        local.sin_addr.s_addr = 0;

        t= setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (t==-1){
                perror("setsockopt fallita");
                return 1;
        }

        if ( -1 == bind(s, (struct sockaddr *)&local, sizeof(struct sockaddr_in))){
                perror("Bind Fallita");
                return -1;
        }

        if ( -1 == listen(s,10)){
                perror("Listen Fallita");
                return -1;
        }

        remote.sin_family = AF_INET;
        remote.sin_port = htons(0);
        remote.sin_addr.s_addr = 0;

        while (1){
                len = sizeof(struct sockaddr_in);
                s2 = accept(s,(struct sockaddr *)&remote,&len);
                bzero(hbuffer,10000);
                bzero(h,200*sizeof(char*));
                reqline = h[0].n = hbuffer;
                for (i=0,j=0; len = read(s2, hbuffer+i, 1); i++){
                        if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
                                hbuffer[i-1]=0;
                                if (!h[j].n[0])
                                        break;
                                h[++j].n=hbuffer + i + 1;
                        }
                        if (hbuffer[i]==':' && !h[j].v){
                                hbuffer[i]=0;
                                h[j].v = hbuffer + i + 1;
                        }
                }

                if(len == -1){
                        perror("Read Fallita");
                        return -1;
                }

                printf("%s\n",reqline); //GET
                for(i=1; i<j; i++)		//resto dell'header
                        printf("%s:%s\n",h[i].n,h[i].v);

                len = strlen(reqline);
                method = reqline;

                for(i=0; i<len && reqline[i]!=' '; i++); reqline[i++]=0;
                url = reqline+i;

                for(;i<len && reqline[i]!=' '; i++); reqline[i++]=0;
                ver = reqline+i;

                if(!strcmp(method, "GET")){
                        filename = url+1;
                        fin = fopen(filename, "rt");
                        if(fin == NULL){
                                sprintf(response,"HTTP/1.1 404 Not Found\r\n\r\n");
                                printf("HTTP/1.1 404 Not Found\n");
                                write(s2,response,strlen(response));
                        }
                        else{
                                match = 1;
                                if(!strncmp(url+1, "secure/", 7)){
                                        match = 0;
                                        authhead = 0;
                                        for(i=1; i<j && !authhead; i++){
                                                if(!strcmp(h[i].n,"Authorization")){
                                                        authhead = 1;
                                                        for(k=1; k<strlen(h[i].v) && h[i].v[k]!=' '; k++);
                                                        auth64 = &h[i].v[k+1];
                                                        fpass = fopen("./secure/pass.txt", "r");
                                                        bzero(rawbuff,1000);
                                                        bzero(in64buff,1500);

                                                        while((fgets(rawbuff, 1000, fpass))!=NULL){
                                                                        cnt = 0;
                                                                        cnt64 = 0;
                                                                        for(ibuff=0; rawbuff[ibuff]!=' '&&ibuff<strlen(rawbuff); ibuff++);
                                                                        rawbuff[ibuff]=':';
                                                                        while(rawbuff[cnt]!='\r'){
                                                                                        cnt++;
                                                                                        in64buff[cnt64++] = converter(rawbuff[cnt-1] >> 2);
                                                                                        prev = (rawbuff[cnt-1]&3) << 4;
                                                                                        if(rawbuff[cnt]!='\r'){
                                                                                                        cnt++;
                                                                                                        in64buff[cnt64++] = converter((rawbuff[cnt-1] >> 4) + prev);
                                                                                                        prev = ((rawbuff[cnt-1]&15) << 2);
                                                                                        }
                                                                                        else break;
                                                                                        if(rawbuff[cnt]!='\r'){
                                                                                                        cnt++;
                                                                                                        in64buff[cnt64++] = converter(((rawbuff[cnt-1] >> 6) + prev));
                                                                                                        in64buff[cnt64++] = converter(((rawbuff[cnt-1]&63)));
                                                                                        }
                                                                                        else break;
                                                                        }
                                                                        cnt = cnt%3;
                                                                        cnt = 3-cnt;
                                                                        if(cnt!=3){
                                                                                in64buff[cnt64++] = converter((prev));
                                                                                if(cnt==2){
                                                                                        in64buff[cnt64++] = '=';
                                                                                        in64buff[cnt64++] = '=';
                                                                                }
                                                                                else
                                                                                        in64buff[cnt64++] = '=';
                                                                        }
                                                                        if(!strcmp(auth64,in64buff)){
                                                                                match=1;
                                                                                break;
                                                                        }
                                                                        bzero(rawbuff,1000);
                                                                        bzero(in64buff,1500);
                                                        }
                                                        fclose(fpass);
                                                }
                                        }
                                        if(!authhead){
                                                sprintf(response,"HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic realm=\"protected\"\r\n\r\n");
                                                printf("HTTP/1.1 401 Authorization Required\n");
                                                write(s2,response,strlen(response));
                                        } //compare pop-up di login al client
                                }
                                if(match){
                                        sprintf(response, "HTTP/1.1 200 OK\r\n\r\n");
                                        printf("HTTP/1.1 200 OK\n");
                                        write(s2, response, strlen(response));
                                        while ((c = fgetc(fin))!=EOF) write(s2,&c,1);
                                }
                                else if(authhead){
                                        sprintf(response, "HTTP/1.1 401 Unauthorized\r\n\r\n");
                                        printf("HTTP/1.1 401 Unauthorized\n");
                                        write(s2, response, strlen(response));
                                }
                        }
                        if(fin!=NULL) //la gran testa di peri di Zingi che non l'ha messo lui
                                fclose(fin);
                }
                else{
                        sprintf(response,"HTTP/1.1 501 Not Implemented\r\n\r\n");
                        printf("HTTP/1.1 501 Not Implemented\n");
                        write(s2,response,strlen(response));
                }
                close(s2);
                printf("\n");
        }
        close(s); //ah ah lol, ti piacerebbe
}
