#include <iostream>
#include "ossie/ossieSupport.h"

#include "SourceSDDS.h"
int main(int argc, char* argv[])
{
    SourceSDDS_i* SourceSDDS_servant;
    Component::start_component(SourceSDDS_servant, argc, argv);
    return 0;
}

