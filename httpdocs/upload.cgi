#!/usr/bin/python3

import sys, os
import urllib

length = os.getenv('CONTENT_LENGTH')

if length:
    postdata = sys.stdin.read(int(length))
    data = postdata.split('\r\n\r\n')
    filename = data[0].split('\r\n')[1].split('; ')[-1][10:-1]
    ans = ''
    if filename: 
        for strs in data[1: ]:
            ans += strs
            ans += '\r\n\r\n'
        #assert (len(ans) == 0)
        dt = ans.split('\r\n')[:-4]
        ans = ''
        for strs in dt:
            ans += strs
            ans += '\r\n'
        with open(filename, "w") as f:
            f.write(ans)

    a = "<html><head><title>POST</title></head><body><h1> UPLOAD SUCCESS! </h1><ul>"
   
    lens = len(a) 
    print("Content-length: " + str(lens) + '\r\n\r\n', end='')
    print(a, end='')
   
else:
    pass

