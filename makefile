
default:
	gcc -o travelMonitor travelMonitor.c -ggdb3
	gcc -o monitor monitor.c -ggdb3

clean:
	rm ./tmp/* travelMonitor monitor
