require 'rest_client'
require 'json'

Before do
  @pid = fork { exec('../src/pincaster pincaster.conf') }
end

After do |scenario|
  Process.kill("HUP", @pid)
  sleep 1
end

Given /^Pincaster is started$/ do
  sleep 1
end

Given /^Layer '(.*)' is created$/ do |layer|
  RestClient.post 'localhost:4269/api/1.0/layers/'+layer+'.json', ''
end

Given /^Record '(.*)' is created in layer '(.*)' with location '(.*)' and properties '(.*)'$/ do |record, layer, location, properties|
  RestClient.put 'localhost:4269/api/1.0/records/'+layer+'/'+record+'.json', location+'&'+properties
end

def capture_api_result
  begin
    result = JSON.parse(yield)
    result.delete('tid')
  rescue Exception=>e
    result = e.class
  end
  return result
end

When /^Client GET (\/api\/.*)$/ do |path|
  @result = capture_api_result { RestClient.get 'localhost:4269'+path }
end

When /^Client GET (\/public\/.*)$/ do |path|
  @result = RestClient.get 'localhost:4269'+path do |response, request|
    @content_type = response.headers[:content_type]
    response.return!
  end
end

When /^Client POST (.*) '(.*)'$/ do |path, content|
  @result = capture_api_result { RestClient.post 'localhost:4269'+path, content }
end

When /^Client DELETE (.*)$/ do |path|
  @result = capture_api_result { RestClient.delete 'localhost:4269'+path }
end

When /^Client PUT (.*) '(.*)'$/ do |path, content|
  @result = capture_api_result { RestClient.put 'localhost:4269'+path, content }
end

Then /^Pincaster returns:$/ do |string|
  expected = JSON.parse(string)
  @result.should == expected
end

Then /^Pincaster returns (.*):$/ do |expected_content_type, expected_result|
  @content_type.should == expected_content_type
  @result.should == expected_result
end

Then /^Pincaster is dead$/ do
  @result.should == RestClient::ServerBrokeConnection
end

Then /^Pincaster throws 404$/ do
  @result.should == RestClient::ResourceNotFound
end
