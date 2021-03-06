#
# Copyright 2014-2016, Intel Corporation
# Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
# pmempool_create/TEST6 -- test for pmempool create command with poolset
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "pmempool_create\TEST6w"
$Env:UNITTEST_NUM = "6w"
# XXX:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their output
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true

. ..\unittest\unittest.ps1

require_fs_type any

setup

#XXX: port poolsets tests
#
#$POOLSET="$DIR\pool.set"
#$POOL1="$DIR\pool.part1"
#$POOL2="$DIR\pool.part2"
#$POOL3="$DIR\pool.part3"
#$POOLS="$POOL1 $POOL2 $POOL3"
#$POOLREP="$DIR\rep.set"
#$REPL1="$DIR\pool.rep.part1"
#$REPL2="$DIR\pool.rep.part2"
#$REPL3="$DIR\pool.rep.part3"
#$REPS="$REPL1 $REPL2 $REPL3"
#
#rm $POOLSET -Force
#create_poolset $POOLSET 32M:"$POOL1":z 32M:"$POOL2":z 32M:"$POOL3":z
#check_file $POOLSET
#
#rm  $POOLREP -Force
#create_poolset $POOLREP 32M:"$POOL1":x 32M:"$POOL2":x 32M:"$POOL3":x	R 32M:"$REPL1":z 32M:"$REPL2":z 32M:"$REPL3":z
#check_file $POOLREP
#
## PMEMBLK
#rm -Path $POOLS -Force
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create blk 512 $POOLSET
#check_files $POOLS
#check_signatures PMEMBLK $POOLS
#
## PMEMLOG
#rm $POOLS -Force
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create log $POOLSET
#check_files $POOLS
#check_signatures PMEMLOG $POOLS
#
## PMEMOBJ
#rm $POOLS -Force
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create --layout=LAYOUT_NAME obj $POOLSET
#check_files $POOL
#check_signatures PMEMOBJ $POOLS
#
## PMEMOBJ with replica
#rm $POOLS -Force
#expect_normal_exit $PMEMPOOL$Env:EXESUFFIX create --layout=LAYOUT_NAME obj $POOLREP
#check_files $POOLS $REPS
#check_signatures PMEMOBJ $POOLS $REPS
#
#rm $POOLS -Force
#rm $REPS -Force
#rm $POOLSET -Force
#rm $POOLREP -Force
pass
