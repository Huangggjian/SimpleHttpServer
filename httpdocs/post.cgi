#!/usr/bin/python3
# coding:utf-8
import sys, os
import urllib

length = os.getenv('CONTENT_LENGTH')

if length:
    postdata = sys.stdin.read(int(length))
    a = "<html><head><title>POST</title></head><body><h2> Your POST data: </h2><ul>"
    b = ""
    for data in postdata.split('&'):
        b += '<li>' + data + '</li>'
    c = "</ul></body></html>"
    lens = len(a) + len(b) + len(c)
    print("Content-length: " + str(lens) + '\r\n\r\n', end='')
    # print (lens + '\r\n\r\n')
    print(a + b + c, end='')

   

else:
    pass



