require 'rest_client'
require 'json'

Before do
  @pid = fork { exec('../src/pincaster pincaster.conf') }
end

Given /^that Pincaster is started$/ do
  sleep 1
end

After do |scenario|
  Process.kill("HUP", @pid)
  sleep 1
end

When /^Client GET (.*)$/ do |path|
  begin
    @result = JSON.parse(RestClient.get 'localhost:4269'+path)
    @result.delete('tid')
  rescue Exception=>e
    @result = e.class
  end
end

When /^Client POST (.*) (.*)$/ do |path, content|
  begin
    @result = JSON.parse(RestClient.post 'localhost:4269'+path, content)
    @result.delete('tid')
  rescue Exception=>e
    @result = e.class
  end
end

When /^Client DELETE (.*)$/ do |path|
  begin
    @result = JSON.parse(RestClient.delete 'localhost:4269'+path)
    @result.delete('tid')
  rescue Exception=>e
    @result = e.class
  end
end

Then /^Pincaster returns:$/ do |string|
  expected = JSON.parse(string)
  @result.should == expected
end

Then /^Pincaster is dead$/ do
  @result.should == RestClient::ServerBrokeConnection
end
