#include "serverClass.h"
#include <iostream>

int main() {
    Server server(8081);
    if (!server.start()) {
        return 1;
    }

    while (true) {
        server.acceptClients();       
    }

    return 0;
}