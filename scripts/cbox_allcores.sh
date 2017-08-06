#!/bin/bash
#Usage: cbox_percore.sh  <event_selection_string> <program>
event_str_prefix=' -e uncore_cbox_'
event_str_postfix='/'$1'/ '

for i in {0..7}
do
event_str+=$event_str_prefix
event_str+=$i
event_str+=$event_str_postfix
done

complete_string='sudo perf_3.6 stat -x, -a '$event_str$2
echo Running: 
echo $complete_string
$complete_string
