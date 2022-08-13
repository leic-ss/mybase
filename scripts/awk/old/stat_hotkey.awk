BEGIN{

}
{
	key=substr($10, 5, length($10)-5);
	rc=int(substr($16, 4, length($16)-4));
	op = $19

	keyNumMap[key]++;
	keyOpNumMap[key, op]++;
	keyRcNumMap[key, rc] ++;
	keyOpRcNumMap[key, rc] ++;
	rcMap[rc]++;
}
END{
    for (i in rcMap)
    {
            print i" : "rcMap[i]
    }

    print ""
    for (key in keyNumMap)
    {
            print "'"key"'" " "keyNumMap[key] | "sort -n -k2 -r | head -n 60"
            #print "'" key "'" " "keyNumMap[key]
    }

    #close("sort -i -k2 -r | head")
}