function getTsIndx(ts)
{
  idx = 1
  if (ts >=0 && ts < 100){
    idx = 1;
  }else if (ts >= 100 && ts < 500){
    idx = 2;
  }else if (ts >= 500 && ts < 1000){
    idx = 3;
  }else if (ts >= 1000 && ts < 5000){
    idx = 4;
  }else if (ts >= 5000 && ts < 10000){
    idx = 5;
  }else if (ts >= 100000){
  	idx = 6;
  }
  return idx;
}

BEGIN{
type=1

#usage: awk -f stat_netserver.awk type=1 tmp.log
}
{
	if (type == 1) {
		total = int(substr($7, 7, length($7) - 7))
		tsIdx=getTsIndx(total);
		mapCostTs[tsIdx]++;
	} else if (type == 2) {
    total = int(substr($7, 7, length($7) - 7))
    tsIdx=getTsIndx(total);
    ts_min=substr($2, 0, 5);
    map2CostTs[ts_min][tsIdx]++;
  }
}
END{
	ts_str = ""
	slen=asorti(mapCostTs, tC);
	for(i=1; i<=slen; i++)
	{
		print sprintf("%2s     %s", tC[i], mapCostTs[tC[i]])
	}

  slen=asorti(map2CostTs, tC);
  for(i=1; i<=slen; i++)
  {
    print sprintf("%s", tC[i])
    slen2=asorti(map2CostTs[tC[i]], tC2);
    for(j=1; j<=slen2; j++)
    {
      print sprintf("%2s     %s", tC2[j], map2CostTs[tC[i]][tC2[j]])
    }
  }
}