BEGIN{
area = ""
}
{
	sarea = substr($9, 6, length($9) - 6)

	if (sarea != area)
	{
		next
	}

	split($8, strs, ".");
	sroom = substr(strs[1], 2, length(strs[1])-1)"."strs[2]"."strs[3]

	ts=sprintf("%s %s", $1, $2)
  	ts=substr(ts, 2, length(ts)-1)

  	roomReqNum[ts]++;
  	rooms[sroom]++;
  	roomTsReqNum[ts, sroom]++;
}
END{
	header = sprintf("%20s", "ts")

	roomArrlen=asorti(rooms, roomArr);
	for (i=1; i<=roomArrlen; i++)
	{
		roomheader = sprintf("%s", roomArr[i])
		header = sprintf("%s | %10s", header, roomheader)
	}
	print header

	roomTsArrlen=asorti(roomReqNum, roomTsArr);
	for (i=1; i<=roomTsArrlen; i++)
	{
		data = sprintf("%s", roomTsArr[i])

		for (j=1; j<=roomArrlen; j++)
		{
			data = sprintf("%20s | %10s", data, roomTsReqNum[roomTsArr[i], roomArr[j]])
		}

		print data
	}
}