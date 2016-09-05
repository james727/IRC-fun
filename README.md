#JIRC
IRC server written in C. Contains implementation and support for a large portion of the IRC protocol, including private messaging, channel creation, user and operator modes, etc.

#Building JIRC
Clone the repository locally and type `make` in the root directory.

#Running JIRC locally
A successful build will create the executable file chirc. To run the server, use the following syntax:

```
./chirc -o {password} -p {port}
```

The `-o` parameter controls the password for gaining global operator mode, and the `-p` parameter controls the port the server will run on. The `-p` parameter is optional; if it's not passed in, it will default to 6667 (the standard IRC port).

#File structure
There are several files of note in the 'src' folder, including:

1. main.c - where most of the server logic code is, from socket setup to message parsing and responses
2. list.c - contains an implementation of a linked list for storing active users and channels, along with some specialized functions for each
3. connection.h - contains the prototype of the struct used to store user data
4. channel.h - contains the prototype of the struct used to store channel data
