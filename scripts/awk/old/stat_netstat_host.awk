BEGIN{

}
{
        split($5, strs, ".");
        split(strs[4], ips, ":")
        host=strs[1]"."strs[2]"."strs[3]"."ips[1];

        hostNumMap[host]++;
        connStats[$6]++;

        recvQNumMap[host] += int($2)
        sendQNumMap[host] += int($3)
}
END{
        header = sprintf("%16s   %8s   %8s   %8s", "host", "num", "recvq", "sendq")
        print header

        hostNumsLen=asorti(hostNumMap, hostNums);
        for(i=1; i<=hostNumsLen; i++)
        {
                data = sprintf("%16s | %8s | %8s | %8s", hostNums[i], hostNumMap[hostNums[i]], recvQNumMap[hostNums[i]], sendQNumMap[hostNums[i]])
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