#!/bin/bash
response_tree=$(curl -s "http://localhost:8081/tree/italy/emilia-romagna")
russi_48026=$(echo $response_tree | jq -r '.["raravenna"].russi["48026"]')
if [[ $russi_48026 == "null" ]]; then
	echo "'russi: { \"48026\": {} }' not found in the response."
	exit 1
fi
trento=$(echo $response_tree | jq -r '.["trento"]')
if [[ trento == "null" ]]; then
	echo "Found Trento in Emilia Romagna ?"
	exit 2
fi
response_positions=$(curl -s "http://localhost:8081/positions/italy/emilia-romagna/raravenna")
russi=$(echo $response_positions | jq -r '. | index("russi")')
if [[ $russi == "null" ]]; then
	echo "Russi not found under ravenna"
	exit 3
fi
er_under_ra=$(echo $response_positions | jq -r '. | index("emilia-romagna")')
if [[ $er_under_ra != "null" ]]; then
	echo "emilia-romagna found under russi ?!"
	exit 4
fi
response_cap=$(curl -s "http://localhost:8081/italiancaps/48026")
if [[ $response_cap != "/italy/emilia-romagna/raravenna/russi" ]]; then
	echo "Russi not found for cap 48026"
	exit 5
fi
http_error=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:8081/italiancaps/00000")
if [ $http_error -ne 404 ]; then 
	echo "Not found expected for cap 0000"
	exit 6
fi

echo "Test OK"
