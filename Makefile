all:
	g++ server.cpp -lsqlite3 -lcurl -o server
	g++ client.cpp -l sqlite3 -o client
