#!/bin/bash

set -x

curl -X POST -d '{"group": "group_1", "master": "192.168.56.101:5199", "slave": "192.168.56.101:5198"}' 127.0.0.1:7800/api/v1/groupAdd 2>/dev/null | jq .

curl -X POST --data-binary @/media/sf_Shares/dataservice/src/proto/config.proto "192.168.56.101:7800/api/v1/tableAdd?table=abcd&group=group_1&area=1" 2</dev/null | jq .

curl 127.0.0.1:7800/api/v1/list 2>/dev/null | jq .