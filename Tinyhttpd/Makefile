all: httpd
LIBS = -lpthread
httpd: httpd.c
	gcc -g -W -Wall -o $@ $< $(LIBS)

clean: 
	rm httpd

