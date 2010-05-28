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
