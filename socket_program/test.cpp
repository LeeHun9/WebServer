#include <strings.h>
#include <string.h>
#include <cstdio>
#include <stdlib.h>

char* url = 0;

void parse(char* text) {
    url = strpbrk(text, " \t");
    int n = strspn(url, " \t");
    printf("%d\n", n);
    //*url++ = '\0';
    //url += n;
    printf("%s\n", url);
    printf("%s\n", text);
}

int main() {
    char* a = new char[strlen("GET /index.html HTTP/1.1") + 1];
    strcpy(a, "GET /index.html HTTP/1.1");
    
    parse(a);
    
    return 0;
}