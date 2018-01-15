This is the codebase of corecraft as was when the project stopped.
Its's filled with problems and bad code, and should not be used in a production server.
A lot of effort was put into the content, so we're putting it up for downloads
if anyone is curious to check it out. The git history has been stripped as it contained
private information.

The server has been tested on Ubuntu compilled with GCC and Clang. It uses various
C++11/14 features and might not compile on Windows with MSVC.

To run the server:
Put scriptdev2 in bindings folder.
Run CMake on the server directory, resolve any dependencies you're missing.
Compile & install with make.
Generate data files with the tools (-DTOOLS=1 for CMake).
Set up the database, found in database/.
Fix the config files.
Run mangosd and realmd.
(setup_test.txt might be useful, its the steps we took to set up our many tests)
