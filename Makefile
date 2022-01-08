all:
	g++ server.cpp -std=c++17 -lsqlite3 -lcurl -o server
	g++ client.cpp -std=c++17 -l sqlite3 -o client
