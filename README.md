#JIRC
IRC server written in C. Contains implementation and support for a large portion of the IRC protocol, including private messaging, channel creation, user and operator modes, etc.

#Building JIRC
Clone the repository locally and type 'make' in the root directory.

#Running JIRC locally
A successful build will create the executable file chirc. To run the server, use the following syntax:

'''
./chirc -o {password} -p {port}
'''

The '-o' parameter controls the password for gaining global operator mode, and the '-p' parameter controls the port the server will run on. The '-p' parameter is optional; if it's not passed in, it will default to 6667 (the standard IRC port).
