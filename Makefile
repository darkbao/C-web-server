http_server:http_business.o reactor.o public_func.o
	g++ http_business.o reactor.o public_func.o -o http_server -std=c++11 -lpthread -g

http_business.o:http_business.cpp http_business.h public_func.h 
	g++ -c http_business.cpp -o http_business.o -std=c++11 -g 

public_func.o:public_func.cpp public_func.h
	g++ -c public_func.cpp -o public_func.o -std=c++11 -g 

reactor.o:reactor.cpp http_business.h threadpool.h public_func.h 
	g++ -c reactor.cpp -o reactor.o -std=c++11  -lpthread -g
	
clean:
	rm -rf *.o http_server
