# Advanced-OS-6378-Projects

## Introduction
This is a project to implement Distributed File System
The project has the following directory structure :

├── client/
│   ├── client.cpp
│   ├── client.h
│   └── Makefile
├── inp_cl1
├── inp_cl2
├── Makefile
├── mserver/
│   ├── Makefile
│   ├── mserver.cpp
│   └── mserver.hpp
├── project2_Problem_Statement.pdf
├── Read.md
├── server/
│   ├── Makefile
│   ├── server.cpp
│   └── server.h
├── testfile1
├── testfile2
└── testfile3

testfile1, testfile2 and testfile3 are 3 text files which contains readable text which is used by the client to populate the data for uploading to the servers.

inp_cl1 and inp_cl2 are the contains information which allow the client to read from the testfiles and upload them to the servers. It consists of multiple lines, each line corresponding to a read/write instruction. The lines have the following structure :
<Filename> <Request Type> <Offset> <Size>
Each field is separated by space. The <Filename> parameter says which file to read from, <Request Type> is either 0 (read) and 1 (write), <Offset> is the offset within the testfile from where to start reading (ignored during write), and <Size> is the amount of data to read or write in bytes.

## Steps to Build the project:

Go the the project root directory and give the following command:
$make

To clean the project, give the following command:
$make clean

The executable will be present in their respective folders

The M-server, server and client can also be built independently by going to the respective folders and giving the command 'make'.

## Steps to test the project

The target environment should have the following folder structure:
root/
├── client
├── inp_cl1
├── inp_cl2
├── mserver
├── s1/
│   ├── server
├── s2/
│   ├── server
├── s3/
│   ├── server
├── testfile1
├── testfile2
└── testfile3

client, mserver and server are the three executables corresponding to the client, the M-Server and the three servers
Each of the following six commands should be executed, from the root/,  in separate terminals, running on separate machines:

$./mserver
$./s1/server 
$./s2/server 
$./s3/server 
$./client
$./client


The chunks, created by the write will be present in a folder with the name of the file uploaded, inside each s1, s2 and s3 folders. The chunks are numbered in each folder as 0, 1, 2 etc.

The order of the above commands is essential. Mainly,  the M-Server should execute first, followed by each of the 3 servers and then the client. 

To pause the servers, type the letter 'p' in the terminal of the corresponding server; to resume, type 'r'.

At the end of execution, the files mserver_ipaddr.txt and server_ipaddr.txt, along with the testfile* folders in each s1, s2 and s3 folders should be deleted, before the next execution. 
To achieve that, the following command can be given :
$rm -rf mserver_ipaddr.txt server_ipaddr.txt s1/testfile* s2/testfile* s3/testfile*


