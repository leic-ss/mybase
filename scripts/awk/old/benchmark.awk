BEGIN{
area = ""
}
{
	timestamp = $5
	count_str = substr($13, 7, length($13)-7)
	split(count_str, count_strs, ":")
	count_str_len=asorti(count_strs, count_str_arr);
	count = 0
	for (i=1; i<=count_str_len; i++)
	{
		split(count_strs[count_str_arr[i]], count_str_arr_s, ",");
		slot = count_str_arr_s[1]
		slot_count = count_str_arr_s[2]

		count += slot_count
	}

	totals[timestamp] += count

	time_str = substr($14, 6, length($14)-6)
	split(time_str, time_strs, ":");
	time_str_len=asorti(time_strs, time_str_arr);

	for (i=1; i<=time_str_len; i++)
	{
		split(time_strs[time_str_arr[i]], time_str_arr_s, ",");
		slot = time_str_arr_s[1]
		slot_count = time_str_arr_s[2]

		if (slot == 0) {
			total_slots[timestamp] += slot_count
		}
	}
}
END{
	totals_arr_len=asorti(totals, totals_arr);
	totals_slots_arr_len=asorti(total_slots, total_slots_arr);
	for (i=1; i<=totals_arr_len; i++)
	{
		print totals_arr[i] " " totals[totals_arr[i]] " " total_slots[totals_arr[i]] " " ( (totals[totals_arr[i]] - total_slots[totals_arr[i]]) * 100 ) / totals[totals_arr[i]] 
	}
}f