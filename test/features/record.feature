Feature: Record API
  Client should be able to edit Pincaster records throught its HTTP API
  Scenario: create/get/delete
    Given Pincaster is started
      And Layer 'tlay' is created
    When Client PUT /api/1.0/records/tlay/home.json 'description=Maison&who=Robert'
    Then Pincaster returns:
      """
      {
              "status": "stored"
      }
      """
    When Client GET /api/1.0/records/tlay/home.json
    Then Pincaster returns:
      """
      {
	      "key": "home",
	      "type": "hash",
	      "properties": {
		      "description": "Maison",
		      "who": "Robert"
	      }
      }
      """
    When Client DELETE /api/1.0/records/tlay/home.json
    Then Pincaster returns:
      """
      {
              "status": "deleted"
      }
      """
    When Client GET /api/1.0/records/tlay/home.json
    Then Pincaster throws 404
  Scenario: create/update/delete properties
    Given Pincaster is started
      And Layer 'restaurants' is created
      When Client PUT /api/1.0/records/restaurants/abcd.json '_loc=48.512,2.243'
      Then Pincaster returns:
      """
      {
              "status": "stored"
      }
      """
      When Client GET /api/1.0/records/restaurants/abcd.json
      Then Pincaster returns:
      """
      {
              "key": "abcd",
              "type": "point",
              "latitude": 48.512,
              "longitude": 2.243
      }
      """
      When Client PUT /api/1.0/records/restaurants/abcd.json 'name=MacDonalds&closed=1&address=blabla&visits=100000'
      Then Pincaster returns:
      """
      {
              "status": "stored"
      }
      """
      When Client GET /api/1.0/records/restaurants/abcd.json
      Then Pincaster returns:
      """
      {
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
      """
      When Client PUT /api/1.0/records/restaurants/abcd.json '_delete:closed=1&_add_int:visits=127'
      Then Pincaster returns:
      """
      {
              "status": "stored"
      }
      """
      When Client GET /api/1.0/records/restaurants/abcd.json
      Then Pincaster returns:
      """
      {
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
      """
