Pincaster
=========

Pincaster is an in-memory, persistent data store for geographic data and key/dictionnary pairs, with replication and a HTTP/JSON interface.

* [Pincaster presentation](http://www.slideshare.net/jedisct1/an-introduction-to-pincaster-4626242)
* [Pincaster git repository](http://github.com/jedisct1/Pincaster)
* [Download snapshots tarballs](http://download.pureftpd.org/pincaster/snapshots/)
* [Download releases tarballs](http://download.pureftpd.org/pincaster/releases/)


Client libraries
----------------

* [Ruby client library](http://github.com/oz/pincaster)
* [NodeJS client library](http://github.com/zyll/pincaster)
* [Python client library](http://github.com/johnnoone/Pincaster)
* [PHP client library](http://github.com/hugdubois/Pincater-php/)


Schema overview
---------------

* A Pincaster database contains a set of independent layers.
* A layer is a namespace and a geographic space (flat or spherical). Each layer stores dynamic records.
* A record is uniquely identified by a name and can hold either:
  1. No related value (*void* type).
  2. A set of key/value pairs (properties, or *hash* type). Values are binary-safe and can be freely and atomically updated and deleted.
  3. A geographic location (*point* type) defined by a latitude and a longitude.
  4. A set of key/value pairs and a geographic location (*point+hash* type).

  A layer can store records of arbitrary sizes and types.
  
  Records can automatically expire.


Installation
------------
    ./configure
    make install

Review the `pincaster.conf` configuration file and run:

    pincaster /path/to/pincaster.conf


Layers
------

* **Registering a new layer:**

    Method: `POST`

    URI: `http://$HOST:4269/api/1.0/layers/(layer name).json`

* **Deleting a layer:**

    Method: `DELETE`

    URI: `http://$HOST:4269/api/1.0/layers/(layer name).json`

* **Retrieving registered layers:**

    Method: `GET`

    URI: `http://$HOST:4269/api/1.0/layers/index.json`


Records
-------

* **Creating or updating a record:**

    Method: `PUT`

    URI: `http://$HOST:4269/api/1.0/records/(layer name)/(record name).json`

  This `PUT` request can contain application/x-www-form-urlencoded data, in order to create/update arbitrary properties.

  Property names starting with an underscore are reserved. In particular:

    * `_delete:(property name)=1` removes a property of the record,
    * `_delete_all=1` removes the whole properties set of the record,
downgrading its type to *void* or *point*,
    * `_add_int:(property name)=(value)` atomically adds (value) to the
property named (property name), creating it if necessary,
    * `_loc:(latitude),(longitude)` adds or updates a geographic position associated with the record.
    * `_expires_at=(unix timestamp)` have the record automatically expire at
this date. If you later want to remove the expiration of a record, just use
`_expires_at=` (empty value) or `_expires_at=0`.

  A single request can create/change/delete any number of properties.

* **Retrieving a record:**

    Method: `GET`

    URI: `http://$HOST:4269/api/1.0/records/(layer name)/(record name).json`

* **Deleting a record:**

    Method: `DELETE`

    URI: `http://$HOST:4269/api/1.0/records/(layer name)/(record name).json`


Geographic search
-----------------

* **Finding records whose location is within a radius:**

    Method: `GET`

    URI: `http://$HOST:4269/api/1.0/search/(layer name)/nearby/(center point).json?radius=(radius, in meters)`

  The center point is defined as `latitude,longitude`.

  Additional arguments can be added to this query:
  
  * `limit=(max number of results that once reached, will return an overflow)`
  * `properties=(0 or 1)` in order to include properties or not in the reply.

* **Finding records whose location is within a rectangle:**

    Method: `GET`

    URI: `http://$HOST:4269/api/1.0/search/(layer name)/in_rect/(l0,L0,l1,L1).json`

  Additional arguments can be added to this query:
  
  * `limit=(max number of results that once reached, will return an overflow)`
  * `properties=(0 or 1)` in order to include properties or not in the reply.


Range queries
-------------

Keys are indexed with a RB-tree, and they are always pre-sorted in lexical order.

Hence, retrieving keys matching a prefix is a very fast operation.

* **Finding keys matching a prefix:**

    Method: `GET`

    URI: `http://$HOST:4269/api/1.0/search/(layer name)/keys/(prefix)*.json`

  This retrieves every key starting with (prefix). If the final * character
is omitted, an exact matching is performed instead of a prefix matching.

  Additional arguments can be added to this query:
  
  * `limit=(max number of results)` - please note that the default
limit is 250 in order to avoid returning kazillions objects in case of
an erroneous query. If you need to retrieve more, don't forget to
explicitly set this argument.
  * `content=(0 or 1)` in order to retrieve only a list of keys, or
full objects.
  * `properties=(0 or 1)` in order to include properties or not in the reply.


Relations through symbolic links
--------------------------------

Relations between records can be stored as properties whose key name starts with `$link:` and whose value is the key of another record.

While retrieving a specific record, and in geographical and range queries, adding `links=1` will automatically traverse links and recursively retrieve every linked record.

The server will take care of avoiding loops and duplicate records.

Results contain an additional property named `$links` containing found symlinked records.


Serving documents
-----------------

Pincaster can also act like a web server and directly serve documents stored
in keys.

In order to be accesible this way, a key:
  
* must have a `$content` property whose value holds the content that has to
be served
* may have a `$content_type` property with the Content-Type.
  
The default Content-Type currently is `text/plain`.
  
* **Public data from layer (layer name) and key (key) is reachable at:**
  
    Method: `GET`

    URI: `http://$HOST:4269/public/(layer name)/(key)`

Having a HTTP proxy between Pincaster and untrusted users is highly
recommended in order to serve public data.  


Misc
----

* **Ping:**

    Method: any

    URI: `http://$HOST:4269/api/1.0/system/ping.json`


* **Shutting the server down:**

    Method: `POST`

    URI: `http://$HOST:4269/api/1.0/system/shutdown.json`


* **Compacting the journal:**

    The on-disk journal is an append-only file that logs every transaction that
creates or changes data.

    In order to save disk space and to speed up the server start up, a
new and optimized journal can be written by a background process. Once
this operation is complete, the new journal will automatically replace the
previous file.

    This is something you might want to run as a cron job.

    Method: `POST`

    URI: `http://$HOST:4269/api/1.0/system/rewrite.json`


Example
-------

Command-line example with *curl*:

Register a new layer called "restaurants":

	$ curl -dx http://diz:4269/api/1.0/layers/restaurants.json
	{
		"tid": 1,
		"status": "created"
	}

Check the list of active layers:

	$ curl http://diz:4269/api/1.0/layers/index.json
	{
		"tid": 2,
		"layers": [
			{
				"name": "restaurants",
				"records": 0,
				"geo_records": 0,
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

	$ curl -XPUT -d'secret_key=supersecret&last_update=2001/07/08' http://diz:4269/api/1.0/records/restaurants/info.json
	{
		"tid": 3,
		"status": "stored"
	}

What does the record look like?

	$ curl http://diz:4269/api/1.0/records/restaurants/info.json
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

	$ curl -XPUT -d'_loc=48.512,2.243' http://diz:4269/api/1.0/records/restaurants/abcd.json
	{
		"tid": 5,
		"status": "stored"
	}

What does the "abcd" record of the "restaurants" layer look like?

	$ curl http://diz:4269/api/1.0/records/restaurants/abcd.json
	{
		"tid": 6,
		"key": "abcd",
		"type": "point",
		"latitude": 48.512,
		"longitude": 2.243
	}

Let's add some properties to this record, like a name, the fact that it's currently closed, an address and an initial number of visits:

	$ curl -XPUT -d'name=MacDonalds&closed=1&address=blabla&visits=100000' http://diz:4269/api/1.0/records/restaurants/abcd.json
	{
		"tid": 7,
		"status": "stored"
	}

Let's check it:

	$ curl http://diz:4269/api/1.0/records/restaurants/abcd.json
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

	$ curl -XPUT -d'_delete:closed=1&_add_int:visits=127' http://diz:4269/api/1.0/records/restaurants/abcd.json
	{
		"tid": 9,
		"status": "stored"
	}

Now let's look for records whose location is near N 48.510 E 2.240, within a 7 kilometers radius:

	$ curl http://diz:4269/api/1.0/search/restaurants/nearby/48.510,2.240.json?radius=7000
	{
		"tid": 10,
		"matches": [
			{
				"distance": 313.502,
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

	$ curl http://diz:4269/api/1.0/search/restaurants/in_rect/48.000,2.000,49.000,3.000.json?properties=0
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

What about adding a HTML document to the abcd record?

    $ curl -XPUT -d'$content=%3Chtml%3E%3Cbody%3E%3Ch1%3EHello+world%3C%2Fh1%3E%3C%2Fbody%3E%3C%2Fhtml%3E&$content_type=text/html' http://diz:4269/api/1.0/records/restaurants/abcd.json

(feel free to replace `$` with `%24` if you feel pedantic. These examples use the unencoded character for the sake of being easily readable).
    
Now point your web browser to:

    http://diz:4269/public/restaurants/abcd
    
And enjoy the web page!    

Now, what about storing a record about Donald Duck, whose favorite restaurant is this very Mac Donald's:

    curl -XPUT -d'first_name=Donald&last_name=Duck&$link:favorite_restaurant=abcd' http://diz:4269/api/1.0/records/restaurants/donald.json

As expected, Donald's record looks like:

    $ curl http://diz:4269/api/1.0/records/restaurants/donald.json
	{
		"tid": 21,
		"key": "donald",
		"type": "hash",
		"properties": {
			"first_name": "Donald",
			"last_name": "Duck",
			"$link:favorite_restaurant": "abcd"
		}
	}

Here's the same query, with a twist: links traversal. Properties starting with `$link` are resolved, retrieved and send back with the query result as a `$links` property:

    $ curl http://diz:4269/api/1.0/records/restaurants/donald.json?links=1
    {
		"tid": 22,
		"key": "donald",
		"type": "hash",
		"properties": {
			"first_name": "Donald",
			"last_name": "Duck",
			"$link:favorite_restaurant": "abcd"
		},
		"$links": {
			"abcd": {
				"key": "abcd",
				"type": "point+hash",
				"latitude": 48.512,
				"longitude": 2.243,
				"properties": {
					"name": "MacDonalds",
					"address": "blabla",
					"visits": "100127",
					"$content": "<html><body><h1>Hello world</h1></body></html>",
					"$content_type": "text/html"
				}
			}
		}
	}

Let's delete the Mac Donald's record:

	$ curl -XDELETE http://diz:4269/api/1.0/records/restaurants/abcd.json
	{
		"tid": 12,
		"status": "deleted"
	}

And shut the server down:

	$ curl -XPOST http://diz:4269/api/1.0/system/shutdown.json

But can it ping?

	$ curl http://diz:4269/api/1.0/system/ping.json

No, it can't any more, boo-ooh!

