Feature: Layer API
  Client should be able to edit Pincaster layers throught its HTTP API
  Scenario: create/get/delete
    Given that Pincaster is started
    When Client POST /api/1.0/layers/tlay.json ''
    Then Pincaster returns:
      """
      {
              "status": "created"
      }
      """
    When Client GET /api/1.0/layers/index.json
    Then Pincaster returns:
      """
      {
              "layers": [
                      {
                              "name": "tlay",
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
      """
    When Client DELETE /api/1.0/layers/tlay.json
    Then Pincaster returns:
      """
      {
              "status": "deleted"
      }
      """
    When Client GET /api/1.0/layers/index.json
    Then Pincaster returns:
      """
      {
              "layers": [
              ]
      }    
      """
