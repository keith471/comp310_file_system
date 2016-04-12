##This is outputted by the fuse wrapper on an attempted write: echo "hello" > /tmp/test0/testing.txt
xmp_getattr:: path = /testing.txt
xmp_create:: filename = /testing.txt
xmp_getattr:: path = /testing.txt

##And if the file already exists
xmp_getattr:: path = /test.txt
xmp_open:: filename = test.txt
xmp_truncate:: filename = /test.txt
xmp_getattr:: path = /test.txt
xmp_write:: filename = test.txt

##This runs on ls:
xmp_getattr:: path = /
xmp_readdir:: path = /

##This runs when you cat an existing file:
xmp_getattr:: path = /test.txt
xmp_open:: filename = test.txt
xmp_read:: filename = test.txt

##You get this when you touch a file, but it does create the file!
touch: setting times of ‘/tmp/test2/testing.txt’: Function not implemented
##And the output is
xmp_getattr:: path = /testing.txt
xmp_create:: filename = /testing.txt
xmp_getattr:: path = /testing.txt
