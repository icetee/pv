#!/bin/bash
#
# Benchmark the read/write performance of pv by looking at the number of
# read() and write() calls and the average amount of data transferred each
# time, as suggested by Ville Herva <Ville.Herva@iki.fi>.
#

test_input=`mktemp /tmp/pvbench1XXXXXX`
strace_output=`mktemp /tmp/pvbench2XXXXXX`

trap "rm -f ${test_input} ${strace_output}" 0

pv=${pv:-./pv}
test -x ${pv} || pv=pv

dd if=/dev/zero of=${test_input} bs=1k count=1k >/dev/null 2>&1

echo -e "Buf(k)\tRate(k)\tReads\tRsize\tWrites\tWsize"

for ((buffer=100; buffer<=1000; buffer+=100)); do
	for ((rate=0; rate<=1000; rate+=100)); do
		rateparm="-L ${rate}k"
		test ${rate} -eq 0 && rateparm=""
		strace -tt -o ${strace_output} \
		  ${pv} ${rateparm} -B ${buffer}k \
		  -f < ${test_input} > /dev/null 2>&1
		rdata=$(
		  awk '$2~/^read\(0,/{c++;t+=$NF}END{print c "\t" t/c}' \
		   ${strace_output}
		)
		wdata=$(
		   awk '$2~/^write\(1,/{c++;t+=$NF}END{print c "\t" t/c}' \
		   ${strace_output}
		)
		echo -e "${buffer}\t${rate}\t${rdata}\t${wdata}"
	done
done

# EOF
