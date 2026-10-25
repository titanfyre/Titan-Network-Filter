#include "../SDK/NetworkFilter.h"
#include <windows.h>
#include <iostream>
int main(){
    std::cout << "Block this process's new outbound UDP connections to port 9000. Enter restores normal traffic.\n";
    try{
        NetworkFilter::Session filter;
        filter.Start();
        filter.SetRules({NetworkFilter::Rule(1,NF_ACTION_BLOCK,17,9000,L"",GetCurrentProcessId())});
        std::cin.get();
    }catch(const std::exception& error){std::cerr << error.what() << '\n';return 1;}
}
