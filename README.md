Please perform following instruction

$ make clean
$ make

server side:
$ ./server 8080 big_set out.txt

   8080: port number
big_set: directory name contains image files
out.txt: log file about the matching results

client side:
$ ./client 127.0.0.1 8080 list_of_filenames.txt 10

127.0.0.1: IP address of server(it means localhost, if you are testing between two pc, you can use its IP address)
     8080: server's port number
list_of_filenames.txt: list of image filename on client side
10       : drop percentage(range: 0~20)

