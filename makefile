CXX ?= g++

server: main.cpp  ./http_conn/http_conn.cpp
	$(CXX) -o server  $^ -lpthread

clean:
	rm  -r server