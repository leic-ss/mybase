BEGIN{
room = ""
sip = 1
eip = 255
area=""
}
{
	sarea = substr($9, 6, length($9) - 6)

	split($8, strs, ".");
	sroom = substr(strs[1], 2, length(strs[1])-1)"."strs[2]"."strs[3]

	ts=sprintf("%s %s", $1, $2)
  	ts=substr(ts, 2, length(ts)-1)

  	if (area != "" && sarea != area)
	{
		next
	}
  	
  	if (room == "")
  	{
  		roomReqNum[ts]++;
  		rooms[sroom]++;
  		roomTsReqNum[ts, sroom]++;
  		
  		next
  	}
  	else if (sroom != room)
	{
		next
	}

	areas[sarea]++;
	totalReqNumMap[ts]++;
	areaTotalReqNumMap[ts, sarea] ++;

	split(strs[4], ipstr, ":")
	ip=int(ipstr[1])

	if (ip < sip || ip > eip)
	{
		next
	}

  	ips[ip]++;
	areaReqNumMap[ts, ip, sarea] ++;
	ipTotalReqNumMap[ts, ip] ++;
}
END{
	header = sprintf("%20s", "ts")

	roomArrlen=asorti(rooms, roomArr);
	for (i=1; i<=roomArrlen; i++)
	{
		roomheader = sprintf("%s", roomArr[i])
		header = sprintf("%s|%10s", header, roomheader)
	}
	print header

	roomTsArrlen=asorti(roomReqNum, roomTsArr);
	for (i=1; i<=roomTsArrlen; i++)
	{
		data = sprintf("%s", roomTsArr[i])

		for (j=1; j<=roomArrlen; j++)
		{
			data = sprintf("%20s|%10s", data, roomTsReqNum[roomTsArr[i], roomArr[j]])
		}

		print data
	}

	if (roomTsArrlen >= 1)
	{
		print ""
	}

	header = sprintf("%20s|%6s", room, "total")

	iplen=asorti(ips, ipArr);
	for (i=1; i<=iplen; i++)
	{
		header = sprintf("%s|%6s", header, ipArr[i])
	}
	print header

	slen=asorti(totalReqNumMap, tsArr);
	for (i=1; i<=slen; i++)
	{
		data = sprintf("%20s|%6s", tsArr[i], totalReqNumMap[tsArr[i]])
		
		for (j=1; j<=iplen; j++)
		{
			data = sprintf("%s|%6s", data, ipTotalReqNumMap[tsArr[i], ipArr[j]])
		}
		print data
	}

	for (a in areas)
	{
		print ""
		areaheader = sprintf("%s (%s %s)", room, a, areas[a])
		header = sprintf("%20s | %6s", areaheader, "total")
		print header

		slen=asorti(totalReqNumMap, tsArr);
		for (i=1; i<=slen; i++)
		{
			data = sprintf("%20s|%6s", tsArr[i], areaTotalReqNumMap[tsArr[i], a])

			for (j=1; j<=iplen; j++)
			{
				data = sprintf("%s|%6s", data, areaReqNumMap[tsArr[i], ipArr[j], a])
			}

			print data
		}
	}

}