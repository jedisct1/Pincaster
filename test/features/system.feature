Feature: System API
 Client should be able to control Pincaster process throught its HTTP API
 Scenario: ping
   Given that Pincaster is started
   When Client GET /api/1.0/system/ping.json
   Then Pincaster returns:
     """
     {
             "pong": "pong"
     }
     """
