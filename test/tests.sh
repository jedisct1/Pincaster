#! /bin/sh

set -ex

cleanup() {
    sleep 5
    kill $pincaster_pid
    rm -f /tmp/pincaster.db
}
trap 'cleanup' 0
../src/pincaster ../pincaster.conf &
pincaster_pid=$!
sleep 5

EXPECTED=$(python -m json.tool <<EOF
{
	"tid": 1,
	"pong": "pong"
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/system/ping.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 2,
        "status": "created"
}
EOF
)
RESULT=$(curl --silent -d'' -XPOST http://localhost:4269/api/1.0/layers/tlay.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 3,
        "layers": [
                {
                        "name": "tlay",
                        "nodes": 0,
                        "type": "geoidal",
                        "distance_accuracy": "fast",
                        "latitude_accuracy": 0.0001,
                        "longitude_accuracy": 0.0001,
                        "bounds": [
                                -180,
                                -180,
                                180,
                                180
                        ]
                }
        ]
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/layers/index.json | python -mjson.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 4,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d 'description=Maison&who=Robert' http://localhost:4269/api/1.0/records/tlay/home.json | python -mjson.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 5,
        "key": "home",
        "type": "hash",
        "properties": {
                "description": "Maison",
                "who": "Robert"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/home.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 6,
        "status": "deleted"
}
EOF
)
RESULT=$(curl --silent -XDELETE http://localhost:4269/api/1.0/records/tlay/home.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 7,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d '_loc=48.60,2.10&description=Macdo' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -mjson.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 8,
        "key": "macdo",
        "type": "point+hash",
        "latitude": 48.6,
        "longitude": 2.1,
        "properties": {
                "description": "Macdo"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 9,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d '_add_int:visits=1' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 10,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d '_add_int:visits=1' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 11,
        "key": "macdo",
        "type": "point+hash",
        "latitude": 48.6,
        "longitude": 2.1,
        "properties": {
                "description": "Macdo",
		"visits": "2"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 12,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d '_delete:visits=1' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 13,
        "key": "macdo",
        "type": "point+hash",
        "latitude": 48.6,
        "longitude": 2.1,
        "properties": {
                "description": "Macdo"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 14,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d '_delete_all=1' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 15,
        "key": "macdo",
        "type": "point",
        "latitude": 48.6,
        "longitude": 2.1
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 16,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d 'description=Macdo' http://localhost:4269/api/1.0/records/tlay/macdo.json | python -mjson.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 17,
        "key": "macdo",
        "type": "point+hash",
        "latitude": 48.6,
        "longitude": 2.1,
        "properties": {
                "description": "Macdo"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/macdo.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 18,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT -d 'superuser=1&score=10&visits=2&_loc=48.502,2.231' http://localhost:4269/api/1.0/records/tlay/place.json | python -mjson.tool)
[ "$RESULT" = "$EXPECTED" ]


EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 19,
        "status": "stored"
}
EOF
)
RESULT=$(curl --silent -XPUT '-d_delete:superuser=1&name=Ross&firstname=John&_add_int:score=-10&_add_int:visits=1&_loc=49,3' http://localhost:4269/api/1.0/records/tlay/place.json | python -mjson.tool)

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 20,
        "key": "place",
        "type": "point+hash",
        "latitude": 49,
        "longitude": 3,
        "properties": {
                "name": "Ross",
                "firstname": "John",
		"score": "0",
	        "visits": "3"
        }
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/records/tlay/place.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 21,
        "matches": [
                {
                        "distance": 3110.84,
                        "key": "macdo",
                        "type": "point+hash",
                        "latitude": 48.6,
                        "longitude": 2.1,
                        "properties": {
                                "description": "Macdo"
                        }
                }
        ]
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/search/tlay/nearby/48.61,2.14.json?radius=20000 | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 22,
        "matches": [
                {
                        "distance": 3110.84,
                        "key": "macdo",
                        "type": "point+hash",
                        "latitude": 48.6,
                        "longitude": 2.1
                }
        ]
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/search/tlay/nearby/48.61,2.14.json?radius=20000\&properties=0 | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 23,
        "matches": [
                {
                        "distance": 3110.84,
                        "key": "macdo",
                        "type": "point+hash",
                        "latitude": 48.6,
                        "longitude": 2.1,
                        "properties": {
                                "description": "Macdo"
                        }
                }
        ]
}
EOF
)
RESULT=$(curl --silent http://localhost:4269/api/1.0/search/tlay/nearby/48.61,2.14.json?radius=200000\&limit=1 | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 24,
        "status": "deleted"
}
EOF
)
RESULT=$(curl --silent -XDELETE http://localhost:4269/api/1.0/layers/tlay.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

EXPECTED=$(python -m json.tool <<EOF
{
        "tid": 25,
        "rewrite": "started"
}
EOF
)
RESULT=$(curl --silent -d'x' http://localhost:4269/api/1.0/system/rewrite.json | python -m json.tool)
[ "$RESULT" = "$EXPECTED" ]

echo "SUCCESS"

