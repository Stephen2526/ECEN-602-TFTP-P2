ECEN602 HW2 Programming Assignment (TFTP protocol implementation)
-----------------------------------------------------------------

Team Number: 17
---------------------------------------
Member 1 # Yuanfei Sun (925001742)

Member 2 # Ishwarya Srinivasan (226002460)

Member 3 # Archana Sasikumar (825008646)


Description:
--------------------

Server:
1) Server is started on the user input IP address and Port number. The server binds to this IP address and Port number permanently to listen for new requests.

2) Whenever the server receives a new request (RRQ Packet), a new socket is crated using the random function generator.

3) The file requested by the user is opened and handle file not exist error. 

4) The file is then sent in blocks of 512bytes and is copied to another file in the client desktop.

5) After that the server sequentially sends each block or packet and waits for its acknowledgement.

6) SELECT structure is used to serve multiple clients.

7) Once the entire file has been read by the client the file is closed, the socket is closed too.

Unix command for compiling server:
------------------------------------------
make server

Unix command for clean server:
------------------------------------------
make clean

Unix command for starting server:
------------------------------------------
./tftpserver 127.0.0.1 SERVER_PORT
