function getTsIndx(ts)
{
  idx = 1
  if (ts >=0 && ts < 5000){
    idx = 1;
  }else if (ts >= 5000 && ts < 10000){
    idx = 2;
  }else if (ts >= 10000 && ts < 50000){
    idx = 3;
  }else if (ts >= 50000 && ts < 100000){
    idx = 4;
  }else if (ts >= 100000 && ts < 300000){
    idx = 5;
  }else if (ts >= 300000 && ts < 500000){
    idx = 6;
  }else if (ts >= 500000 && ts < 1000000){
    idx = 7;
  }else if (ts >= 1000000){
    idx = 8;
  }

  return idx;
}

function getSizeIdx(size)
{
  idx = 1
  if (size >=0 && size < 5000){
    idx = 1;
  }else if (size >= 5000 && size < 10000){
    idx = 2;
  }else if (size >= 10000 && size < 50000){
    idx = 3;
  }else if (size >= 50000 && size < 100000){
    idx = 4;
  }else if (size >= 100000 && size < 300000){
    idx = 5;
  }else if (size >= 300000 && size < 500000){
    idx = 6;
  }else if (size >= 500000 && size < 1000000){
    idx = 7;
  }else if (size >= 1000000){
    idx = 8;
  }

  return idx;
}

function format(header, mapTsNum, mapTsPutNum, mapTsGetNum, mapNumIdxs, mapTotalNumIdxs, mapPutNumIdxs, mapGetNumIdxs, descs)
{
  headers = sprintf("%20s | %17s", header, "Total")

  tsNumSlen=asorti(mapTsNum, tsNums);

  idxsSlen=asorti(mapNumIdxs, idxs);
  for(i=1; i<=idxsSlen; i++)
  {
    headers = sprintf("%s | %17s", headers, descs[idxs[i]])
  }

  print headers
  for(i=1; i<=tsNumSlen; i++)
  {
    optotalstr = sprintf("%5s %5s %5s", mapTsNum[tsNums[i]], mapTsPutNum[tsNums[i]], mapTsGetNum[tsNums[i]])
    str = sprintf("%20s | %17s", tsNums[i], optotalstr)
    for(j=1; j<=idxsSlen; j++)
    {
      opsizestr = sprintf("%5s %5s %5s", mapTotalNumIdxs[idxs[j], tsNums[i]], mapPutNumIdxs[idxs[j], tsNums[i]], mapGetNumIdxs[idxs[j], tsNums[i]])
      
      str = sprintf("%s | %17s", str, opsizestr)
    }
    
    print str
  }
}

BEGIN{
#FS=", "
sizeDesc[1] = "0K ~ 5K";
sizeDesc[2] = "5K ~ 10K";
sizeDesc[3] = "10K ~ 50K";
sizeDesc[4] = "50K ~ 100K";
sizeDesc[5] = "100K ~ 300K";
sizeDesc[6] = "300K ~ 500K";
sizeDesc[7] = "500K ~ 1000K";
sizeDesc[8] = "1000K ~ ";

tsDesc[1] = "0ms ~ 5ms";
tsDesc[2] = "5ms ~ 10ms";
tsDesc[3] = "10ms ~ 50ms";
tsDesc[4] = "50ms ~ 100ms";
tsDesc[5] = "100ms ~ 300ms";
tsDesc[6] = "300ms ~ 500ms";
tsDesc[7] = "500ms ~ 1000ms";
tsDesc[8] = "1000ms ~ ";
}
{
  ts=sprintf("%s %s", $1, $2)
  ts=substr(ts, 2, length(ts)-1)
  area = substr($9, 6, length($9)-6)

  size=int(substr($11, 7, length($11)-7));
  sizeIdx = getSizeIdx(size);

  costTs=int(substr($14, 5, length($14)-5));
  tsIdx=getTsIndx(costTs);

  op = $19
  if (op == "put")
  {
    mapTsNumPut[ts]++;
    areaMapTsNumPut[area, ts]++;

    mapSizeTsPutIdxs[sizeIdx, ts]++;
    areaMapSizeTsPutIdxs[area, sizeIdx, ts]++;

    mapCostTsPutIdxs[tsIdx, ts]++;
    areaMapCostTsPutIdxs[area, tsIdx, ts]++;
  }
  else if (op == "get")
  {
    mapTsNumGet[ts]++;
    areaMapTsNumGet[area, ts]++;

    mapSizeTsGetIdxs[sizeIdx, ts]++;
    areaMapSizeTsGetIdxs[area, sizeIdx, ts]++;

    mapCostTsGetIdxs[tsIdx, ts]++;
    areaMapCostTsGetIdxs[area, tsIdx, ts]++;
  }
  else
  {
    next;
  }

  totalOpNum++;
  areas[area]++;
  mapTsNum[ts]++;
  areaMapTsNum[area, ts]++;

  mapSizeIdxs[sizeIdx]++;
  mapSizeTsIdxs[sizeIdx, ts]++;
  areaMapSizeTsIdxs[area, sizeIdx, ts]++;

  mapCostTs[tsIdx]++;
  mapCostTsIdxs[tsIdx, ts]++;
  areaMapCostTsIdxs[area, tsIdx, ts]++;

}
END{
  slen=asorti(mapTsNum, tA);

  #header1 = sprintf("%20s %8s %8s %8s", "Ts", "Total", "Put", "Get")
  #print header1

  #for(i=1; i<=slen; i++)
  #{
  #  str = sprintf("%20s %8s %8s %8s", tA[i], mapTsNum[tA[i]], mapTsNumPut[tA[i]], mapTsNumGet[tA[i]])
  #  print str
  #}

  #for (tid in areas)
  #{
  #  print ""

  #  areaheader = sprintf("Ts(%s %s)", tid, areas[tid])
  #  header2 = sprintf("%20s %8s %8s %8s", areaheader, "Total", "Put", "Get")
  #  print header2
  #  for(i=1; i<=slen; i++)
  #  {
  #    str = sprintf("%20s %8s %8s %8s", tA[i], areaMapTsNum[tid, tA[i]], areaMapTsNumPut[tid, tA[i]], areaMapTsNumGet[tid, tA[i]])
  #    print str
  #  }
  #}

  totalheader = sprintf("Ts(%s)", totalOpNum)

  header3 = sprintf("%20s", "Total")
  total3 = sprintf("%20s", totalOpNum)
  slen2=asorti(mapSizeIdxs, tB);
  for(i=1; i<=slen2; i++)
  {
    header3 = sprintf("%s | %17s", header3, sizeDesc[tB[i]])
    total3 = sprintf("%s | %17s", total3, mapSizeIdxs[tB[i]])
  }

  print header3
  print total3


  tstotalheader = sprintf("Ts(%s)", totalOpNum)
  tsheader = sprintf("%20s | %17s", tstotalheader, "Total")

  print ""
  header3 = sprintf("%20s", "Total")
  total3 = sprintf("%20s", totalOpNum)
  slen3=asorti(mapCostTs, tC);
  for(i=1; i<=slen3; i++)
  {
    header3 = sprintf("%s | %17s", header3, tsDesc[tC[i]])
    total3 = sprintf("%s | %17s", total3, mapCostTs[tC[i]])
  }

  print header3
  print total3

  print ""
  format(totalheader, mapTsNum, mapTsNumPut, mapTsNumGet, mapSizeIdxs, mapSizeTsIdxs, mapSizeTsPutIdxs, mapSizeTsGetIdxs, sizeDesc)
  format(totalheader, mapTsNum, mapTsNumPut, mapTsNumGet, mapCostTs, mapCostTsIdxs, mapCostTsPutIdxs, mapCostTsGetIdxs, tsDesc)

  for (tid in areas)
  {
    print ""

    areaheader = sprintf("Ts(%s %s)", tid, areas[tid])

    for(i=1; i<=slen; i++)
    {
      areaMapTsNumTmp[tA[i]] = areaMapTsNum[tid, tA[i]]
      areaMapTsNumPutTmp[tA[i]] = areaMapTsNumPut[tid, tA[i]]
      areaMapTsNumGetTmp[tA[i]] = areaMapTsNumGet[tid, tA[i]]

      for(j=1; j<=slen2; j++)
      {
        areaMapSizeTsIdxsTmp[tB[j], tA[i]] = areaMapSizeTsIdxs[tid, tB[j], tA[i]]
        areaMapSizeTsPutIdxsTmp[tB[j], tA[i]] = areaMapSizeTsPutIdxs[tid, tB[j], tA[i]]
        areaMapSizeTsGetIdxsTmp[tB[j], tA[i]] = areaMapSizeTsGetIdxs[tid, tB[j], tA[i]]
      }

      for(j=1; j<=slen3; j++)
      {
        areaMapCostTsIdxsTmp[tC[j], tA[i]] = areaMapCostTsIdxs[tid, tC[j], tA[i]]
        areaMapCostTsPutIdxsTmp[tC[j], tA[i]] = areaMapCostTsPutIdxs[tid, tC[j], tA[i]]
        areaMapCostTsGetIdxsTmp[tC[j], tA[i]] = areaMapCostTsGetIdxs[tid, tC[j], tA[i]]
      }
    }

    format(areaheader, areaMapTsNumTmp, areaMapTsNumPutTmp, areaMapTsNumGetTmp, mapSizeIdxs, areaMapSizeTsIdxsTmp, areaMapSizeTsPutIdxsTmp, areaMapSizeTsGetIdxsTmp, sizeDesc)

    format(areaheader, areaMapTsNumTmp, areaMapTsNumPutTmp, areaMapTsNumGetTmp, mapCostTs, areaMapCostTsIdxsTmp, areaMapCostTsPutIdxsTmp, areaMapCostTsGetIdxsTmp, tsDesc)
  }
  
}