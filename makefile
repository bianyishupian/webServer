CXX ?= g++

server: main.cpp ./m_server/server.cpp  ./http_conn/http_conn.cpp ./c_log/log.cpp ./timer/timer.cpp ./config/config.cpp
	$(CXX) -o server  $^ -lpthread -g

clean:
	rm  -r server