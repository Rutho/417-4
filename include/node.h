#include <sys/socket.h>
#include <netinet/in.h>
typedef struct Node {

	struct in_addr ipAddr;
	Node predecessor;
	Node sucessor;

} Node;
