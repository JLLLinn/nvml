#
# Copyright 2014-2016, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# pmempool_rm/TEST1 -- test for pmempool rm
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_rm\TEST1w"
$Env:UNITTEST_NUM = "1w"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

. ..\unittest\unittest.ps1

require_fs_type any

setup

$LOG="out$Env:UNITTEST_NUM.log"

rm $LOG -Force -Recurse -ea si

#XXX: port poolsets tests
#
## Create pmemlog, pmemblk and pmemobj pools
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create -m400 log $DIR/pool.log
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create -m440 blk 512 $DIR/pool.blk
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create -m444 obj $DIR/pool.obj
#
## Create poolset with replica
#create_poolset $DIR/pool.set 32M:$DIR/pool.part1:z 32M:$DIR/pool.part2:z\
#	R 32M:$DIR/rep.part1:z 32M:$DIR/rep.part2:z
#
## Create pmemobj pools from the poolset
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create -m444 obj $DIR/pool.set
#
#chmod 444 $DIR/pool.set $DIR/pool.part1 $DIR/pool.part2 $DIR/rep.part1 $DIR/rep.part2
#
## Check if all pools and poolset exist
#check_files\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set\
#	$DIR/pool.part1\
#	$DIR/pool.part2\
#	$DIR/rep.part1\
#	$DIR/rep.part2
#
#echo -e "n\nn\nn\ny\ny\ny\ny\n" | expect_normal_exit $PMEMPOOL$Env:EXESUFFIX rm -sv\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set >> $LOG
#
#check_files\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set\
#
#check_no_files\
#	$DIR/pool.part1\
#	$DIR/pool.part2\
#	$DIR/rep.part1\
#	$DIR/rep.part2
#
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create -m444 obj $DIR/pool.set
#
#check_files\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set\
#	$DIR/pool.part1\
#	$DIR/pool.part2\
#	$DIR/rep.part1\
#	$DIR/rep.part2
#
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX rm -fv\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set >> $LOG
#
#check_no_files\
#	$DIR/pool.log\
#	$DIR/pool.blk\
#	$DIR/pool.obj\
#	$DIR/pool.set\
#	$DIR/pool.part1\
#	$DIR/pool.part2\
#	$DIR/rep.part1\
#	$DIR/rep.part2
#check

pass
