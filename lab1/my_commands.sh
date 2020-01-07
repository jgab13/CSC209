  #!/usr/bin/env bash

./echo_arg csc209 > echo_out.txt
./echo_stdin < echo_out.txt
./count 209 | wc -m
ls -S | ./echo_stdin
