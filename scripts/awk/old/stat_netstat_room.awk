BEGIN{

}
{
	split($5, strs, ".");
	room=strs[1]"."strs[2]"."strs[3];

	roomNumMap[room]++;
	connStats[$6]++;

	recvQNumMap[room] += int($2)
	sendQNumMap[room] += int($3)
}
END{
	header = sprintf("%16s   %8s   %8s   %8s", "room", "num", "recvq", "sendq")
	print header

	for (r in roomNumMap)
	{
		data = sprintf("%16s | %8s | %8s | %8s", r, roomNumMap[r], recvQNumMap[r], sendQNumMap[r])
		print data
	}

	print ""

	header = sprintf("%16s   %8s", "stats", "num");
	print header
	for (s in connStats)
	{
		data = sprintf("%16s | %8s", s, connStats[s]);
		print data
	}
}