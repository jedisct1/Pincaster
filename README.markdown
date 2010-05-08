Pinecaster
==========

Pinecaster is an in-memory, persistent data store for geographic data and key/value pairs, with a HTTP/JSON interface.

Schema overview
---------------

* A Pinecaster database contains a set of independent layers.
* A layer is a namespace and a geographic space (flat or spherical). Each layer stores dynamic records.
* A record is uniquely identified by a name and can hold either:
  1. No related value (*void* type).
  2. A set of key/value pairs (properties, or *hash* type). Values are binary-safe and can be freely and atomically updated and deleted.
  3. A geographic location (*point* type) defined by a latitude and a longitude.
  4. A set of key/value pairs and a geographic location (*point+hash* type).

  A layer can store records of arbitrary sizes and types.

Installation
------------
    ./autogen.sh
    ./configure
    make install

Review the `pinecaster.conf` configuration file and run:

    pinecaster /path/to/pinecaster.conf

Layers
------

* **Registering a new layer:**

    Method: `POST`

    URI: `http://$HOST:8080/api/1.0/layers/(layer name).json`

* **Deleting a layer:**

    Method: `DELETE`

    URI: `http://$HOST:8080/api/1.0/layers/(layer name).json`

* **Retrieving registered layers:**

    Method: `GET`

    URI: `http://$HOST:8080/api/1.0/layers/index.json`

Records
-------

* **Creating or updating a record:**

    Method: `PUT`

    URI: `http://$HOST:8080/api/1.0/records/(layer name)/(record name).json`

    This `PUT` request can contain application/x-www-form-urlencoded data, in order to create/update arbitrary properties.

    Property names starting with an underscore are reserved. In particular:

  * `_delete:(property name)=1` removes a property of the record,
  * `_delete_all=1` removes the whole properties set of the record,
downgrading its type to *void* or *point*,
  * `_add_int:(property name)=(value)` atomically adds (value) to the
property named (property name), creating it if necessary,
  * `_loc:(latitude),(longitude)` adds or updates a geographic position associated with the record.

    A single request can create/change/delete any number of properties.

* **Retrieving a record:**

    Method: `GET`

    URI: `http://$HOST:8080/api/1.0/records/(layer name)/(record name).json`

* **Deleting a record:**

    Method: `DELETE`

    URI: `http://$HOST:8080/api/1.0/records/(layer name)/(record name).json`

Geographic search
-----------------

* **Finding records whose location is within a radius:**

    Method: `GET`

    URI: `http://$HOST:8080/api/1.0/search/(layer name)/nearby/(center point).json?radius=(radius, in meters)`

  The center point is defined as `latitude,longitude`.

  Additional arguments can be added to this query:
  
  * `limit=(max number of results)`
  * `properties=(0 or 1)` in order to include properties or not in the reply.

* **Finding records whose location is within a rectangle:**

    Method: `GET`

    URI: `http://$HOST:8080/api/1.0/search/(layer name)/in_rect/(l0,L0,l1,L1).json`

  Additional arguments can be added to this query:
  
  * `limit=(max number of results)`
  * `properties=(0 or 1)` in order to include properties or not in the reply.

Misc
----

* **Ping:**

    Method: any

    URI: `http://$HOST:8080/api/1.0/system/ping.json`


* **Shutting the server down:**

    Method: `POST`

    URI: `http://$HOST:8080/api/1.0/system/shutdown.json`


* **Compacting the journal:**

    The on-disk journal is an append-only file that logs every
transaction that creates or changes data.

    In order to save disk space and to speed up the server start up, a
new and optimized journal can be written as a background process. Once
this operation is complete, the new journal will automatically replace the
previous file.

    This is something you might want to run as a cron job.

    Method: `POST`

    URI: `http://$HOST:8080/api/1.0/system/rewrite.json`

    __Warning: this command is still under construction.__

Example
-------

Command-line example with *curl*:

Register a new layer called "restaurants":

	$ curl -dx http://diz:8080/api/1.0/layers/restaurants.json
	{
		"tid": 1,
		"status": "created"
	}

Check the list of active layers:

	$ curl http://diz:8080/api/1.0/layers/index.json
	{
		"tid": 2,
		"layers": [
			{
				"name": "restaurants",
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

Now let's add a hash record called "info". Just a set of key/values with no geographic data:

	$ curl -XPUT -d'secret_key=supersecret&last_update=2001/07/08' http://diz:8080/api/1.0/records/restaurants/info.json
	{
		"tid": 3,
		"status": "stored"
	}

What does the record look like?

	$ curl http://diz:8080/api/1.0/records/restaurants/info.json
	{
		"tid": 4,
		"key": "info",
		"type": "hash",
		"properties": {
			"secret_key": "supersecret",
			"last_update": "2001/07/08"
		}
	}

Let's add a McDonald's, just with geographic data:

	$ curl -XPUT -d'_loc=48.512,2.243' http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 5,
		"status": "stored"
	}

What does the "abcd" record of the "restaurants" layer look like?

	$ curl http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 6,
		"key": "abcd",
		"type": "point",
		"latitude": 48.512,
		"longitude": 2.243
	}

Let's add some properties to this record, like a name, the fact that it's currently closed, an address and an initial number of visits:

	$ curl -XPUT -d'name=MacDonalds&closed=1&address=blabla&visits=100000' http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 7,
		"status": "stored"
	}

Let's check it:

	$ curl http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 8,
		"key": "abcd",
		"type": "point+hash",
		"latitude": 48.512,
		"longitude": 2.243,
		"properties": {
			"name": "MacDonalds",
			"closed": "1",
			"address": "blabla",
			"visits": "100000"
		}
	}

Atomically delete the "closed" property from this record and add 127 visits:

	$ curl -XPUT -d'_delete:closed=1&_add_int:visits=127' http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 9,
		"status": "stored"
	}

Now let's look for records whose location is near N 48.510 E 2.240, within a 7 kilometers radius:

	$ curl http://diz:8080/api/1.0/search/restaurants/nearby/48.510,2.240.json?radius=7000
	{
		"tid": 10,
		"matches": [
			{
				"distance": 2199.7,
				"key": "abcd",
				"type": "point+hash",
				"latitude": 48.512,
				"longitude": 2.243,
				"properties": {
					"name": "MacDonalds",
					"address": "blabla",
					"visits": "100127"
				}
			}
		]
	}

And what's in a rectangle, without properties?

	$ curl http://diz:8080/api/1.0/search/restaurants/in_rect/48.000,2.000,49.000,3.000.json?properties=0
	{
		"tid": 11,
		"matches": [
			{
				"distance": 20254.9,
				"key": "abcd",
				"type": "point+hash",
				"latitude": 48.512,
				"longitude": 2.243
			}
		]
	}

Let's delete the record:

	$ curl -XDELETE http://diz:8080/api/1.0/records/restaurants/abcd.json
	{
		"tid": 12,
		"status": "deleted"
	}

And shut the server down:

	$ curl -XPOST http://diz:8080/api/1.0/system/shutdown.json

But can it ping?

	$ curl http://diz:8080/api/1.0/system/ping.json

No, it can't any more, boo-ooh!

