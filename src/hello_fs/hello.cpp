// See FUSE: example/hello.c

#include "hello_fs.hpp"

int main(int argc, char *argv[])
{

    HelloFS fs;

    const auto status = fs.run(argc, argv);

    return status;
}