all : test1 test2 test_repeat

test1 : test1.cpp
	g++ -o test1 test1.cpp -pthread -lleveldb
test2 : test2.cpp
	g++ -o test2 test2.cpp -pthread -lleveldb
test_repeat : test_repeat.cpp
	g++ -o test_repeat test_repeat.cpp -pthread -lleveldb

clean : 
	rm test1
