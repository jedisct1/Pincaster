Feature: Public API
  Pincaster can also act like a web server and directly serve documents stored in keys.
  Scenario: get
    Given Pincaster is started
    And Layer 'restaurants' is created
    And Record 'abcd' is created in layer 'restaurants' with location '_loc=48.512,2.243' and properties 'name=MacDonalds&address=blabla&visits=100000&$content=<html><body><h1>Hello world</h1></body></html>&$content_type=text/html'
    When Client GET /public/restaurants/abcd
    Then Pincaster returns text/html:
    """
    <html><body><h1>Hello world</h1></body></html>
    """
