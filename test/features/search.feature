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
  Scenario: keys
    Given Pincaster is started
    And Layer 'restaurants' is created
    And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000'
    And Record 'abce' is created in layer 'restaurants' with location '_loc=48.612,2.343' and properties 'name=MacDonalds2&address=blabla2&visits=200000'
    And Record 'abde' is created in layer 'restaurants' with location '_loc=48.712,2.443' and properties 'name=MacDonalds3&address=blabla3&visits=300000'
    When Client GET /api/1.0/search/restaurants/keys/abc*.json
      Then Pincaster returns:
      """
      {
              "matches": [
                      {
                              "key": "abcd",
                              "type": "point+hash",
                              "latitude": 48.512,
                              "longitude": 2.243,
                              "properties": {
                                      "name": "MacDonalds",
                                      "address": "blabla",
                                      "visits": "100000"
                              }
                      },
                      {
                              "key": "abce",
                              "type": "point+hash",
                              "latitude": 48.612,
                              "longitude": 2.343,
                              "properties": {
                                      "name": "MacDonalds2",
                                      "address": "blabla2",
                                      "visits": "200000"
                              }
                      }
              ]
      }
      """
  Scenario: keys content=0
    Given Pincaster is started
    And Layer 'restaurants' is created
    And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000'
    And Record 'abce' is created in layer 'restaurants' with location '_loc=48.612,2.343' and properties 'name=MacDonalds2&address=blabla2&visits=200000'
    And Record 'abde' is created in layer 'restaurants' with location '_loc=48.712,2.443' and properties 'name=MacDonalds3&address=blabla3&visits=300000'
    When Client GET /api/1.0/search/restaurants/keys/abc*.json?content=0
      Then Pincaster returns:
      """
      {
              "keys": [ "abcd", "abce" ]
      }
      """
  Scenario: keys content=0 limit=1
    Given Pincaster is started
    And Layer 'restaurants' is created
    And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000'
    And Record 'abce' is created in layer 'restaurants' with location '_loc=48.612,2.343' and properties 'name=MacDonalds2&address=blabla2&visits=200000'
    And Record 'abde' is created in layer 'restaurants' with location '_loc=48.712,2.443' and properties 'name=MacDonalds3&address=blabla3&visits=300000'
    When Client GET /api/1.0/search/restaurants/keys/abc*.json?content=0&limit=1
      Then Pincaster returns:
      """
      {
              "keys": [ "abcd" ]
      }
      """
