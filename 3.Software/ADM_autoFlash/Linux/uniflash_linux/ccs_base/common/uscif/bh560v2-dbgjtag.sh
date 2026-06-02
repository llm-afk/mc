#!/bin/bash
tmp_log_file=/tmp/bh560v2-output.log

./dbgjtag $* >$tmp_log_file 2>&1

exit_code=$?

echo >>$tmp_log_file
echo DBGJTAG exited with code $exit_code >>$tmp_log_file

exit $exit_code
