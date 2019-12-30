# makefile for myhttpd.c & myhttp.c

all: myhttp myhttpd

myhttp: myhttp.c
	gcc myhttp.c -o myhttp

myhttpd: myhttpd.c
	gcc myhttpd.c -o myhttpd