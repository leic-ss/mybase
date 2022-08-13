BEGIN{

}
{
	split($8, strs, ".");
	room=substr(strs[1], 2, length(strs[1])-1)"."strs[2]"."strs[3];

	reqNumMap[room]++;
}
END{
	header = sprintf("%16s   %8s", "room", "num")
	print header

	for (r in reqNumMap)
	{
		data = sprintf("%16s | %8s", r, reqNumMap[r])
		print data
	}
}