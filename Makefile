http_server:http_business.o reactor.o 
	g++ http_business.o reactor.o -o http_server -std=c++11 -lpthread

http_business.o:http_business.cpp http_business.h
	g++ -c http_business.cpp -o http_business.o -std=c++11 

reactor.o:reactor.cpp http_business.h threadpool.h locker.h 
	g++ -c reactor.cpp -o reactor.o -std=c++11  -lpthread
	
clean:
	rm -rf *.o http_server
