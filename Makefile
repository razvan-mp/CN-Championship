all:
	g++ client.cpp -l sqlite3 -o client
	g++ server.cpp -l sqlite3 -o server