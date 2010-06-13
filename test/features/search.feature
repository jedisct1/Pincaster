Feature: Search API
  Client should be able to search Pincaster records throught its HTTP API
  Scenario: neary
    Given Pincaster is started
      And Layer 'restaurants' is created
      And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000'
      When Client GET /api/1.0/search/restaurants/nearby/48.510,2.240.json?radius=7000
      Then Pincaster returns:
      """
      {
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
                                      "visits": "100000"
                              }
                      }
              ]
      }
      """
  Scenario: in_rect
    Given Pincaster is started
      And Layer 'restaurants' is created
      And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000'
      When Client GET /api/1.0/search/restaurants/in_rect/48.000,2.000,49.000,3.000.json?properties=0
      Then Pincaster returns:
      """
      {
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
      """
