#in makefiles, a hash indicates a comment instead
all: ipcamera
ipcamera: ipcamera.c
	gcc -o ipcamera ipcamera.c -lcurl
